#include "anaiero/sdk/native_mesh.hpp"
#include "anaiero/infrastructure/native_tls.hpp"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>

namespace anaiero::sdk::mesh {
namespace tls=::anaiero::mesh::tls;
using TransportCode=::anaiero::mesh::TransportCode;
namespace {
constexpr std::string_view ready="CEVMESH/2 READY\n";
std::uint64_t now(){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());}
Result fail(Code code,std::string text){return {code,std::move(text)};}
std::string number(std::uint64_t value){return std::to_string(value);}
bool integer(std::string_view text,std::uint64_t& value){const auto result=std::from_chars(text.data(),text.data()+text.size(),value);return !text.empty()&&result.ec==std::errc{}&&result.ptr==text.data()+text.size()&&number(value)==text;}
std::span<const std::byte> bytes(std::string_view value){return std::as_bytes(std::span(value.data(),value.size()));}
tls::Configuration credentials(const NativeCredentials& value){return {value.certificate_file,value.private_key_file,value.authority_file,"anaiero-mesh/2"};}
AuthenticatedIdentity identity(const tls::Connection& connection){const auto& peer=connection.peer();return {std::string(peer.certificate_sha256()),peer.certificate_expiry_unix_seconds(),peer.verified()};}
Result transport(TransportCode code){
  if(code==TransportCode::ok)return {};
  const auto result=code==TransportCode::cancelled?Code::cancelled:code==TransportCode::timeout?Code::deadline:
    code==TransportCode::unverified_peer||code==TransportCode::tls_error||code==TransportCode::protocol_error?Code::unauthorized:Code::io_error;
  return fail(result,::anaiero::mesh::transport_code_name(code));
}
Result write(tls::Connection& connection,std::string_view value,std::uint32_t timeout){return transport(connection.write_frame(bytes(value),timeout));}
Result read(tls::Connection& connection,Bytes& value,std::uint32_t timeout){
  std::vector<std::byte> frame;const auto result=connection.read_frame(frame,timeout);
  value.assign(reinterpret_cast<const char*>(frame.data()),frame.size());return transport(result);
}
Bytes response(const Result& result){return pack({"error",number(static_cast<std::uint32_t>(result.code)),result.message});}
Result remote_error(const std::vector<std::string>& fields){
  std::uint64_t code=0;if(fields.size()!=3||fields[0]!="error"||!integer(fields[1],code)||code>static_cast<std::uint32_t>(Code::deadline))
    return fail(Code::corrupt,"invalid canonical mesh control response");
  return {static_cast<Code>(code),fields[2]};
}
Result fields(tls::Connection& connection,std::vector<std::string>& output,std::uint32_t timeout){
  Bytes value;if(auto result=read(connection,value,timeout);!result)return result;
  if(auto result=unpack(value,output,32,65536);!result)return result;
  if(!output.empty()&&output[0]=="error")return remote_error(output);
  return {};
}
Result execute(host::IStoreTaskExecutor& executor,host::StoreTask task){
  struct Completion {std::mutex mutex;std::condition_variable changed;bool done=false;Result result;};
  auto completion=std::make_shared<Completion>();
  auto admitted=executor.submit(std::move(task),[completion](Result result){
    {std::lock_guard lock(completion->mutex);completion->result=std::move(result);completion->done=true;}
    completion->changed.notify_one();
  });
  if(!admitted)return admitted;
  // Once admitted, captures live until exactly one completion. A network
  // cancellation cannot discard an unknown canonical commit outcome.
  std::unique_lock lock(completion->mutex);completion->changed.wait(lock,[&]{return completion->done;});return completion->result;
}
bool permanently_blocked(Code code){return code==Code::unauthorized||code==Code::expired||code==Code::conflict||code==Code::corrupt||
  code==Code::history_expired||code==Code::rebootstrap_required||code==Code::archive_required||code==Code::archive_unavailable||code==Code::unsupported;}
std::string hex(std::string_view value){static constexpr char alphabet[]="0123456789abcdef";std::string out;out.reserve(value.size()*2);for(unsigned char c:value){out.push_back(alphabet[c>>4]);out.push_back(alphabet[c&15]);}return out;}
bool unhex(std::string_view value,Bytes& out){
  out.clear();if(value.size()%2)return false;
  auto nibble=[](char c)->int{return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
  for(std::size_t i=0;i<value.size();i+=2){const auto high=nibble(value[i]),low=nibble(value[i+1]);if(high<0||low<0)return false;out.push_back(static_cast<char>(high*16+low));}return true;
}
Result hello(tls::Connection& connection,tls::Context& context,const NativeSubscriberOptions& options,Session& session,host::IStoreTaskExecutor& executor){
  if(auto result=transport(connection.connect(context,{options.address,options.port},options.server_certificate_sha256,options.connect_timeout_ms));!result)return result;
  Bytes banner;if(auto result=read(connection,banner,options.io_timeout_ms);!result)return result;
  if(banner!=ready)return fail(Code::unsupported,"peer did not negotiate CEVMESH/2");
  const auto peer=identity(connection);
  if(auto result=execute(executor,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.authenticate(peer,options.remote_principal,now(),session);});!result)return result;
  if(auto result=write(connection,pack({"hello",options.local_principal}),options.io_timeout_ms);!result)return result;
  std::vector<std::string> reply;if(auto result=fields(connection,reply,options.io_timeout_ms);!result)return result;
  return reply==std::vector<std::string>{"authenticated"}?Result{}:fail(Code::corrupt,"invalid authentication response");
}
}

struct NativeServer::Impl {
  struct Job {
    std::uint64_t connection=0;std::string principal,source;TransferCost cost;bool control=false;
    host::StoreTask task;std::mutex mutex;std::condition_variable changed;bool done=false;Result result;
  };
  struct Client {tls::Connection connection;std::thread worker;bool occupied=false,finished=false,authenticated=false;};
  host::IStoreTaskExecutor& executor;NativeServerOptions options;tls::Context context;tls::Listener listener;
  mutable std::mutex mutex;std::condition_variable changed,available;std::thread acceptor,owner;
  std::vector<std::unique_ptr<Client>> clients;std::deque<std::shared_ptr<Job>> controls;
  std::map<std::uint64_t,std::shared_ptr<Job>> jobs;FairScheduler scheduler;
  NativeServerStatus view;std::atomic<bool> stopping{false};bool started=false;
  Impl(host::IStoreTaskExecutor& value,NativeServerOptions bounds):executor(value),options(std::move(bounds)),
    scheduler({options.maximum_clients+options.maximum_handshakes,32768,64,16U*1024U*1024U+65536U,16U*1024U*1024U+65536U,8192,8192}){}
  void notify(std::uint64_t generation) noexcept {
    {std::lock_guard lock(mutex);if(generation<=view.availability_generation)return;
      view.availability_generation=generation;for(auto& client:clients)client->connection.notify_activity();}
    available.notify_all();
  }
  Result submit(std::size_t index,std::string principal,std::string source,TransferCost cost,bool control,host::StoreTask task){
    auto job=std::make_shared<Job>();job->connection=index+1;job->principal=std::move(principal);job->source=std::move(source);job->cost=cost;job->control=control;job->task=std::move(task);
    {
      std::lock_guard lock(mutex);if(stopping.load())return fail(Code::cancelled,"mesh stopping");
      if(control){if(controls.size()>=clients.size())return fail(Code::capacity,"bounded mesh control reserve full");controls.push_back(job);}
      else {
        const auto queued=scheduler.enqueue({job->connection,job->principal,job->source},cost);if(!queued)return queued;
        jobs.emplace(job->connection,job);
      }
      view.queued_work=controls.size()+jobs.size();
    }
    changed.notify_all();std::unique_lock lock(job->mutex);job->changed.wait(lock,[&]{return job->done;});return job->result;
  }
  void owner_loop() noexcept {
    std::size_t control_burst=0;
    for(;;){
      std::shared_ptr<Job> job;
      {
        std::unique_lock lock(mutex);changed.wait(lock,[&]{return stopping.load()||!controls.empty()||!jobs.empty();});++view.owner_wakeups;
        if(stopping.load()&&controls.empty()&&jobs.empty())break;
        if(!controls.empty()&&(control_burst<4||jobs.empty())){job=std::move(controls.front());controls.pop_front();++control_burst;}
        else {
          std::uint64_t key=0;
          // Bounded credit rounds cover the largest admitted artifact; these
          // touch only bounded scheduler metadata and never poll the store.
          for(unsigned round=0;round<1024&&!scheduler.take(key);++round){}
          if(key){job=std::move(jobs.at(key));jobs.erase(key);control_burst=0;}
          else if(!jobs.empty()){job=std::move(jobs.begin()->second);scheduler.cancel(job->connection);jobs.erase(job->connection);job->result=fail(Code::capacity,"work cost cannot be scheduled");}
        }
        view.queued_work=controls.size()+jobs.size();
      }
      if(!job)continue;
      Result result;
      if(stopping.load())result=fail(Code::cancelled,"mesh stopping");
      else if(!job->result)result=job->result;
      else try {
        std::uint64_t generation=0;
        result=execute(executor,[&](evidence::ICommitStore& store){const auto completed=job->task(store);generation=store.info().generation;return completed;});
        notify(generation);
      }catch(...){result=fail(Code::io_error,"mesh storage task raised an exception");}
      {std::lock_guard lock(job->mutex);job->result=std::move(result);job->done=true;}job->changed.notify_one();
    }
  }
  Result streaming(std::size_t index,const Session& session,const std::vector<std::string>& request){
    lifecycle::Subscription sub;std::uint64_t generation=0,epoch=0,lease=0,sequence=0;
    if(request.size()!=8||!integer(request[2],generation)||!integer(request[4],epoch)||!integer(request[5],lease)||!integer(request[6],sequence))
      return fail(Code::invalid,"invalid subscription request");
    sub={request[1],session.principal,{request[3],epoch},generation,lease,lifecycle::SubscriptionPhase::active,{sequence,request[7]},{},false};
    if(auto result=submit(index,session.principal,sub.scope.uid,{1,1},true,[&](evidence::ICommitStore& store){Service service(store,options.service);auto opened=service.subscribe(session,sub,now());if(!opened.committed())return opened;lifecycle::Service lifecycle(store);return lifecycle.subscription(sub.id,sub);});!result.committed())return result;
    auto& connection=clients[index]->connection;
    if(auto result=write(connection,pack({"subscribed",number(sub.acknowledged.sequence),sub.acknowledged.payload_sha256}),options.io_timeout_ms);!result)return result;
    while(!stopping.load()){
      std::uint64_t observed=0;{std::lock_guard lock(mutex);observed=view.availability_generation;}
      Delivery page;TransferCost charged;
      const auto next=submit(index,session.principal,sub.scope.uid,{2*options.service.maximum_page_bytes,3*options.service.maximum_page_observations+2*options.service.maximum_page_bytes/4096+16},false,
        [&](evidence::ICommitStore& store){Service service(store,options.service);return service.next(session,sub.id,sub.generation,now(),page,charged);});
      if(next.code==Code::completed){
        const auto deadline=std::min(sub.lease_until_unix_s,session.authority_expiry_unix_s);
        while(!stopping.load()){
          {std::lock_guard lock(mutex);if(view.availability_generation!=observed)break;}
          const auto instant=now();if(instant>=deadline)return fail(Code::expired,"subscription or peer authority expired while waiting");
          const auto remaining=std::min<std::uint64_t>(60000,(deadline-instant)*1000);bool readable=false;
          const auto activity=connection.wait_activity(static_cast<std::uint32_t>(remaining),readable);
          if(activity==TransportCode::timeout)continue; // Lifecycle/socket timer, no store read.
          if(activity!=TransportCode::ok)return transport(activity);
          if(readable){Bytes unexpected;const auto input=read(connection,unexpected,options.io_timeout_ms);
            return input?fail(Code::invalid,"unexpected data while subscription has no outstanding offer"):input;}
          break;
        }
        continue;
      }
      if(!next)return next;
      // Successful authority completion is the handoff boundary. Later
      // revocation cannot retract bytes already handed to this transport.
      if(auto result=submit(index,session.principal,sub.scope.uid,{1,8},true,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.authorize_delivery(session,page,now());});!result)return result;
      const auto payload=encode_delivery(page);if(auto result=write(connection,payload,options.io_timeout_ms);!result)return result;
      {std::lock_guard lock(mutex);++view.delivered_pages;view.delivered_bytes+=payload.size();view.charged_read_bytes+=charged.bytes;}
      std::vector<std::string> ack;if(auto result=fields(connection,ack,options.io_timeout_ms);!result)return result;
      std::uint64_t ack_generation=0,ack_sequence=0;
      if(ack.size()!=4||ack[0]!="ack"||!integer(ack[1],ack_generation)||!integer(ack[2],ack_sequence)||ack_generation!=page.generation||
        lifecycle::Cursor{ack_sequence,ack[3]}!=page.to)return fail(Code::conflict,"ACK differs from exact offered page");
      const auto acknowledged=submit(index,session.principal,sub.scope.uid,{1,1},true,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.acknowledge(session,page,now());});
      if(!acknowledged.committed())return acknowledged;
      {std::lock_guard lock(mutex);++view.acknowledged_pages;}
      if(auto result=write(connection,pack({"acknowledged",number(page.to.sequence),page.to.payload_sha256}),options.io_timeout_ms);!result)return result;
    }
    return fail(Code::cancelled,"mesh stopped");
  }
  Result command(std::size_t index,const Session& session,std::vector<std::string>& request){
    auto& connection=clients[index]->connection;
    if(request.empty())return fail(Code::invalid,"empty mesh command");
    if(request[0]=="subscribe")return streaming(index,session,request);
    if(request[0]=="artifact"){
      std::uint64_t epoch=0,offset=0;if(request.size()!=5||!integer(request[2],epoch)||!integer(request[4],offset))return fail(Code::invalid,"invalid artifact request");
      const lifecycle::SourceRef source{request[1],epoch};ArtifactChunk chunk;TransferCost cost;
      const auto result=submit(index,session.principal,source.uid,{options.service.maximum_artifact_bytes+options.service.maximum_page_bytes,8+options.service.maximum_artifact_bytes/4096},false,
        [&](evidence::ICommitStore& store){Service service(store,options.service);return service.artifact_chunk(session,source,request[3],offset,options.service.maximum_page_bytes,now(),chunk,cost);});
      if(!result)return result;
      if(auto authorized=submit(index,session.principal,source.uid,{1,8},true,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.authorize_artifact(session,source,chunk.digest,now());});!authorized)return authorized;
      {std::lock_guard lock(mutex);view.charged_read_bytes+=cost.bytes;view.delivered_bytes+=chunk.bytes.size();}
      return write(connection,pack({"artifact",chunk.digest,number(chunk.offset),number(chunk.total_bytes),chunk.bytes}),options.io_timeout_ms);
    }
    if(request[0]=="bootstrap"){
      std::uint64_t generation=0,epoch=0,lease=0;if(request.size()!=6||!integer(request[2],generation)||!integer(request[4],epoch)||!integer(request[5],lease))return fail(Code::invalid,"invalid bootstrap request");
      const lifecycle::SourceRef source{request[3],epoch};BootstrapOffer offer;
      const auto result=submit(index,session.principal,source.uid,{2*options.service.maximum_artifact_bytes+65536,16+2*options.service.maximum_artifact_bytes/4096},false,
        [&](evidence::ICommitStore& store){Service service(store,options.service);return service.offer_bootstrap(session,request[1],generation,source,lease,now(),offer);});
      if(!result.committed())return result;
      const auto encoded=encode_bootstrap(offer);const auto digest=sha256(encoded);
      if(auto authorized=submit(index,session.principal,source.uid,{1,8},true,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.authorize_bootstrap(session,offer,now());});!authorized)return authorized;
      if(auto sent=write(connection,pack({"bootstrap",digest,number(encoded.size())}),options.io_timeout_ms);!sent)return sent;
      for(std::size_t offset=0;offset<encoded.size();offset+=options.service.maximum_page_bytes){
        std::vector<std::string> next;if(auto received=fields(connection,next,options.io_timeout_ms);!received)return received;
        if(next!=std::vector<std::string>{"chunk",number(offset)})return fail(Code::conflict,"bootstrap chunk cursor mismatch");
        const auto chunk=encoded.substr(offset,options.service.maximum_page_bytes);
        if(auto authorized=submit(index,session.principal,source.uid,{chunk.size()+64,8+chunk.size()/4096},false,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.authorize_bootstrap(session,offer,now());});!authorized)return authorized;
        if(auto sent=write(connection,pack({"chunk",number(offset),chunk}),options.io_timeout_ms);!sent)return sent;
        {std::lock_guard lock(mutex);view.delivered_bytes+=chunk.size();}
      }
      std::vector<std::string> installed;if(auto received=fields(connection,installed,options.io_timeout_ms);!received)return received;
      if(installed!=std::vector<std::string>{"installed",offer.id,offer.checkpoint_sha256})return fail(Code::conflict,"bootstrap installation receipt mismatch");
      const auto accepted=submit(index,session.principal,source.uid,{1,1},true,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.accept_bootstrap(session,offer,now());});
      if(!accepted.committed())return accepted;
      return write(connection,pack({"bootstrap-accepted",offer.id}),options.io_timeout_ms);
    }
    if(request[0]=="bootstrap-status"){
      std::uint64_t generation=0,sequence=0;if(request.size()!=6||!integer(request[2],generation)||!integer(request[4],sequence))return fail(Code::invalid,"invalid bootstrap status request");
      const auto result=submit(index,session.principal,"bootstrap-status",{1,8},true,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.bootstrap_accepted(session,request[1],generation,request[3],{sequence,request[5]},now());});
      if(!result)return result;
      return write(connection,pack({"bootstrap-accepted",request[1],request[2],request[3]}),options.io_timeout_ms);
    }
    return fail(Code::unsupported,"unsupported CEVMESH/2 command");
  }
  void client_loop(std::size_t index) noexcept {
    auto& client=*clients[index];Result result;
    try {
      result=transport(client.connection.complete_server_handshake(context,options.handshake_timeout_ms));
      {
        std::lock_guard lock(mutex);--view.handshakes;
        if(result&&!stopping.load()&&view.clients<options.maximum_clients){client.authenticated=true;++view.clients;++view.accepted_connections;}
        else {++view.rejected_connections;if(result)result=fail(Code::capacity,"authenticated client capacity reached");}
      }
      changed.notify_all();
      if(result){
        result=write(client.connection,ready,options.io_timeout_ms);
        std::vector<std::string> hello_;if(result)result=fields(client.connection,hello_,options.io_timeout_ms);
        Session session;
        if(result&&(hello_.size()!=2||hello_[0]!="hello"||!valid_token(hello_[1],256)))result=fail(Code::invalid,"invalid hello");
        if(result){const auto peer=identity(client.connection);result=submit(index,hello_[1],"authentication",{1,1},true,
          [&](evidence::ICommitStore& store){Service service(store,options.service);return service.authenticate(peer,hello_[1],now(),session);});}
        if(result)result=write(client.connection,pack({"authenticated"}),options.io_timeout_ms);
        while(result&&!stopping.load()){
          std::vector<std::string> request;result=fields(client.connection,request,options.io_timeout_ms);
          if(result)result=command(index,session,request);
        }
      }
      if(!result&&client.connection.connected()&&!stopping.load())(void)write(client.connection,response(result),options.io_timeout_ms);
    }catch(...){result=fail(Code::io_error,"native mesh connection exception");}
    client.connection.close();
    {std::lock_guard lock(mutex);if(client.authenticated)--view.clients;client.finished=true;}
    changed.notify_all();
  }
  void accept_loop() noexcept {
    while(!stopping.load()){
      std::size_t index=clients.size();
      {
        std::unique_lock lock(mutex);changed.wait(lock,[&]{return stopping.load()||(view.handshakes<options.maximum_handshakes&&std::any_of(clients.begin(),clients.end(),[](const auto& client){return !client->occupied||client->finished;}));});
        if(stopping.load())break;
        for(std::size_t i=0;i<clients.size();++i)if(!clients[i]->occupied||clients[i]->finished){index=i;break;}
        clients[index]->occupied=true;clients[index]->finished=false;clients[index]->authenticated=false;
      }
      auto& client=*clients[index];if(client.worker.joinable())client.worker.join();
      const auto accepted=listener.accept_socket(client.connection,60000);
      if(accepted==TransportCode::ok&&!stopping.load()){
        {std::lock_guard lock(mutex);++view.handshakes;}
        try{client.worker=std::thread([this,index]{client_loop(index);});continue;}catch(...){std::lock_guard lock(mutex);--view.handshakes;}
      }
      client.connection.close();{std::lock_guard lock(mutex);client.occupied=false;}
      if(stopping.load()||accepted==TransportCode::cancelled)break;
      if(accepted!=TransportCode::timeout){request_stop();break;}
    }
  }
  void request_stop() noexcept {
    stopping.store(true);{std::lock_guard lock(mutex);view.running=false;for(auto& client:clients)client->connection.cancel();}
    listener.cancel();changed.notify_all();available.notify_all();
  }
};
NativeServer::NativeServer(host::IStoreTaskExecutor& executor,NativeServerOptions options):impl_(std::make_unique<Impl>(executor,std::move(options))){}
NativeServer::~NativeServer(){stop();}
Result NativeServer::start(){
  auto& value=*impl_;if(value.started)return fail(Code::busy,"mesh server already started");
  if(!value.options.maximum_clients||value.options.maximum_clients>32||!value.options.maximum_handshakes||value.options.maximum_handshakes>16||
      !tls::detail::valid_timeout(value.options.handshake_timeout_ms)||!tls::detail::valid_timeout(value.options.io_timeout_ms)||
      !value.options.service.maximum_page_bytes||value.options.service.maximum_page_bytes>24576||!value.options.service.maximum_page_observations||
      value.options.service.maximum_page_observations>32||!value.options.service.maximum_artifact_bytes||value.options.service.maximum_artifact_bytes>8U*1024U*1024U)
    return fail(Code::invalid,"native mesh operating bounds invalid");
  if(auto result=transport(value.context.open(credentials(value.options.credentials),tls::Role::server));!result)return result;
  if(auto result=transport(value.listener.open(value.context,{value.options.address,value.options.port}));!result)return result;
  std::vector<std::unique_ptr<Impl::Client>> clients;for(std::size_t i=0;i<value.options.maximum_clients+value.options.maximum_handshakes;++i)clients.push_back(std::make_unique<Impl::Client>());
  value.stopping.store(false);
  {std::lock_guard lock(value.mutex);value.clients=std::move(clients);const auto generation=value.view.availability_generation;value.view={};value.view.availability_generation=generation;
    value.view.address=value.listener.local_endpoint().address;value.view.port=value.listener.local_endpoint().port;value.view.running=true;value.started=true;}
  try{value.owner=std::thread([&value]{value.owner_loop();});value.acceptor=std::thread([&value]{value.accept_loop();});}
  catch(...){stop();return fail(Code::io_error,"native mesh thread creation failed");}
  return {};
}
void NativeServer::notify_commit(std::uint64_t generation) noexcept{impl_->notify(generation);}
NativeServerStatus NativeServer::status()const {std::lock_guard lock(impl_->mutex);return impl_->view;}
void NativeServer::request_stop()noexcept{impl_->request_stop();}
void NativeServer::stop()noexcept {
  auto& value=*impl_;if(!value.started)return;value.request_stop();
  if(value.acceptor.joinable())value.acceptor.join();
  for(auto& client:value.clients)if(client->worker.joinable())client->worker.join();
  if(value.owner.joinable())value.owner.join();
  value.listener.close();
  {std::lock_guard lock(value.mutex);value.started=false;value.view.running=false;}
}

Result read_native_server_options(std::string_view path,NativeServerOptions& options){
  if(path.empty()||path.size()>4096||path.find('\0')!=std::string_view::npos)return fail(Code::invalid,"invalid mesh configuration path");
  const std::filesystem::path native_path(std::u8string(reinterpret_cast<const char8_t*>(path.data()),path.size()));
  std::ifstream file(native_path,std::ios::binary);if(!file)return fail(Code::io_error,"cannot open mesh configuration");
  std::string line;if(!std::getline(file,line))return fail(Code::invalid,"empty mesh configuration");
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line!="ANAIEROMESH/2")return fail(Code::invalid,"mesh configuration needs ANAIEROMESH/2");
  std::set<std::string> seen;NativeServerOptions result;std::size_t total=line.size();
  while(std::getline(file,line)){
    total+=line.size();if(total>32768||line.find('\0')!=std::string::npos)return fail(Code::capacity,"mesh configuration exceeds bounds");
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty()||line[0]=='#')continue;
    const auto separator=line.find('=');if(separator==std::string::npos)return fail(Code::invalid,"mesh setting must be key=value");
    const auto key=line.substr(0,separator),text=line.substr(separator+1);if(!seen.insert(key).second)return fail(Code::invalid,"duplicate mesh setting");
    if(key=="certificate")result.credentials.certificate_file=text;
    else if(key=="private_key")result.credentials.private_key_file=text;
    else if(key=="authority")result.credentials.authority_file=text;
    else if(key=="address")result.address=text;
    else {std::uint64_t value=0;if(!integer(text,value))return fail(Code::invalid,"invalid numeric mesh setting");
      if(key=="port"&&value<=65535)result.port=static_cast<std::uint16_t>(value);
      else if(key=="clients"&&value<=32)result.maximum_clients=static_cast<std::size_t>(value);
      else if(key=="handshakes"&&value<=16)result.maximum_handshakes=static_cast<std::size_t>(value);
      else if(key=="handshake_timeout_ms"&&value<=60000)result.handshake_timeout_ms=static_cast<std::uint32_t>(value);
      else if(key=="io_timeout_ms"&&value<=60000)result.io_timeout_ms=static_cast<std::uint32_t>(value);
      else return fail(Code::invalid,"unknown or out-of-range mesh setting");
    }
  }
  if(file.bad())return fail(Code::io_error,"mesh configuration read failed");
  options=std::move(result);return {};
}
static Result administer_store(evidence::ICommitStore& store,std::string_view command,Bytes& output){
  output.clear();if(command.empty()||command.size()>4090||command.find('\0')!=std::string_view::npos)return fail(Code::invalid,"invalid bounded mesh administration request");
  std::istringstream input{std::string(command)};input.imbue(std::locale::classic());std::vector<std::string> words;
  for(std::string word;input>>word;){if(words.size()>=40)return fail(Code::capacity,"too many administration fields");words.push_back(std::move(word));}
  if(words.empty())return fail(Code::invalid,"empty mesh administration request");
  const auto apply=[&]()->Result {
    Service mesh(store);lifecycle::Service lifecycle(store);
    if(words[0]=="scope"){
      std::uint64_t epoch=0;if(words.size()!=3||!integer(words[2],epoch))return fail(Code::invalid,"scope SOURCE EPOCH");
      lifecycle::Source source;if(auto status=lifecycle.source({words[1],epoch},source);!status)return status;
      output=hex(encode_identity(source.identity))+"\n";return {};
    }
    if(words[0]=="peer"){
      std::uint64_t generation=0,expected=0,expiry=0;
      if(words.size()!=8||!integer(words[3],generation)||!integer(words[4],expected)||!integer(words[5],expiry))return fail(Code::invalid,"peer PRINCIPAL CERT_SHA GENERATION EXPECTED EXPIRY active|suspended|revoked OPERATION");
      lifecycle::PeerPhase phase;
      if(words[6]=="active")phase=lifecycle::PeerPhase::active;else if(words[6]=="suspended")phase=lifecycle::PeerPhase::suspended;
      else if(words[6]=="revoked")phase=lifecycle::PeerPhase::revoked;else return fail(Code::invalid,"invalid peer phase");
      return lifecycle.upsert_peer({words[1],words[2],generation,expiry,phase},expected,now(),words[7]);
    }
    if(words[0]=="grant"){
      std::uint64_t generation=0,expected=0;Bytes encoded;EngineIdentity source;
      if(words.size()<8||!unhex(words[2],encoded)||!decode_identity(encoded,source)||!integer(words[3],generation)||!integer(words[4],expected))
        return fail(Code::invalid,"grant PRINCIPAL CONTEXT_HEX GENERATION EXPECTED send|receive|both|closed bootstrap|ordinary OPERATION [ARTIFACT_SHA...]");
      if(words[5]!="send"&&words[5]!="receive"&&words[5]!="both"&&words[5]!="closed")return fail(Code::invalid,"invalid mesh grant direction");
      if(words[6]!="bootstrap"&&words[6]!="ordinary")return fail(Code::invalid,"invalid bootstrap grant");
      Grant grant{words[1],source,generation,words[5]=="send"||words[5]=="both",words[5]=="receive"||words[5]=="both",words[6]=="bootstrap",{}};
      grant.artifacts.assign(words.begin()+8,words.end());return mesh.configure_grant(grant,expected,words[7]);
    }
    if(words[0]=="cancel-bootstrap"){
      if(words.size()!=3)return fail(Code::invalid,"cancel-bootstrap OFFER_ID OPERATION");
      return mesh.cancel_bootstrap(words[1],words[2]);
    }
    if(words[0]=="receiver-retention-review"){
      std::uint64_t epoch=0,first=0,last=0;
      if(words.size()<8||!integer(words[4],epoch)||!integer(words[5],first)||!integer(words[6],last))
        return fail(Code::invalid,"receiver-retention-review PLAN PRINCIPAL SOURCE EPOCH FIRST LAST OPERATION [artifact:SHA|bootstrap:ID|receipt:SHA...]");
      ReceiverRetentionRequest request{words[1],words[2],{words[3],epoch},first,last,{},{}};
      for(std::size_t i=8;i<words.size();++i){
        if(words[i].starts_with("artifact:"))request.artifacts.push_back(words[i].substr(9));
        else if(words[i].starts_with("bootstrap:"))request.bootstrap_ids.push_back(words[i].substr(10));
        else if(words[i].starts_with("receipt:"))request.delivery_receipts.push_back(words[i].substr(8));
        else return fail(Code::invalid,"receiver retention object requires artifact:, bootstrap: or receipt: prefix");
      }
      ReceiverRetentionPlan plan;const auto result=mesh.review_receiver_retention(request,words[7],plan);
      if(result.committed()){
        const auto phase=plan.phase==lifecycle::MaintenancePhase::reviewed?"reviewed":plan.phase==lifecycle::MaintenancePhase::activated?"activated":"completed";
        output="RECEIVER_RETENTION "+plan.request.id+' '+phase+" GENERATION "+number(plan.reviewed_generation)+
          " OBJECTS "+number(plan.objects.size())+" BYTES "+number(plan.expected_bytes)+" ROOT "+plan.reviewed_root+"\n";
      }
      return result;
    }
    if(words[0]=="receiver-retention-status"){
      if(words.size()!=2)return fail(Code::invalid,"receiver-retention-status PLAN");
      ReceiverRetentionPlan plan;if(auto result=mesh.receiver_retention(words[1],plan);!result)return result;
      const auto phase=plan.phase==lifecycle::MaintenancePhase::reviewed?"reviewed":plan.phase==lifecycle::MaintenancePhase::activated?"activated":"completed";
      output="RECEIVER_RETENTION "+plan.request.id+' '+phase+" GENERATION "+number(plan.reviewed_generation)+" OBJECTS "+number(plan.objects.size())+
        " BYTES "+number(plan.expected_bytes)+" ROOT "+plan.reviewed_root+"\n";return {};
    }
    if(words[0]=="receiver-retention-activate"||words[0]=="receiver-retention-reclaim"){
      if(words.size()!=3)return fail(Code::invalid,"receiver-retention-activate|receiver-retention-reclaim PLAN OPERATION");
      return words[0]=="receiver-retention-activate"?mesh.activate_receiver_retention(words[1],words[2]):mesh.reclaim_receiver_retention(words[1],words[2]);
    }
    if(words[0]=="received"){
      std::uint64_t epoch=0;if(words.size()!=5||!integer(words[4],epoch))return fail(Code::invalid,"received PRINCIPAL SUBSCRIPTION SOURCE EPOCH");
      ReceiveReceipt receipt;if(auto status=mesh.received(words[1],words[2],{words[3],epoch},receipt);!status)return status;
      output="RECEIVED "+number(receipt.generation)+" "+number(receipt.cursor.sequence)+" "+(receipt.cursor.payload_sha256.empty()?"-":receipt.cursor.payload_sha256)+" "+receipt.receipt_sha256+"\n";return {};
    }
    return fail(Code::unsupported,"unknown local mesh administration command");
  };
  const auto result=apply();
  if(result.committed()&&output.empty())output="OK "+std::string(code_name(result.code))+"\n";
  return result;
}
Result administer_native(host::IStoreTaskExecutor& executor,std::string_view command,Bytes& output){
  return execute(executor,[&](evidence::ICommitStore& store){return administer_store(store,command,output);});
}
Result submit_native_administration(host::IStoreTaskExecutor& executor,std::string command,std::function<void(Result,Bytes)> completion){
  if(command.empty()||command.size()>4090||command.find('\0')!=std::string::npos||!completion)return fail(Code::invalid,"invalid bounded mesh administration request");
  struct Owned {std::string command;Bytes output;};auto state=std::make_shared<Owned>();state->command=std::move(command);
  return executor.submit([state](evidence::ICommitStore& store){return administer_store(store,state->command,state->output);},
    [state,completion=std::move(completion)](Result result){completion(std::move(result),std::move(state->output));});
}

struct NativeSubscriber::Impl {
  host::IStoreTaskExecutor& executor;NativeSubscriberOptions options;tls::Context context;tls::Connection connection;
  mutable std::mutex mutex;std::condition_variable changed;std::thread worker;std::atomic<bool> stopping{false};NativeSubscriberStatus view;
  Impl(host::IStoreTaskExecutor& value,NativeSubscriberOptions bounds):executor(value),options(std::move(bounds)){}
  Result run_connection(){
    Session session;if(auto result=hello(connection,context,options,session,executor);!result)return result;
    {std::lock_guard lock(mutex);++view.connections;}
    auto sub=options.subscription;ReceiveReceipt receipt;
    const auto received=execute(executor,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.received(options.remote_principal,sub.id,sub.scope,receipt);});
    if(received)sub.acknowledged=receipt.cursor;else if(received.code!=Code::not_found)return received;
    if(auto result=write(connection,pack({"subscribe",sub.id,number(sub.generation),sub.scope.uid,number(sub.scope.epoch),number(sub.lease_until_unix_s),number(sub.acknowledged.sequence),sub.acknowledged.payload_sha256}),options.io_timeout_ms);!result)return result;
    std::vector<std::string> subscribed;if(auto result=fields(connection,subscribed,options.io_timeout_ms);!result)return result;
    std::uint64_t acknowledged=0;
    if(subscribed.size()!=3||subscribed[0]!="subscribed"||!integer(subscribed[1],acknowledged))return fail(Code::corrupt,"invalid subscription response");
    if(received&&options.through_sequence&&receipt.cursor.sequence>=options.through_sequence&&acknowledged==receipt.cursor.sequence&&subscribed[2]==receipt.cursor.payload_sha256){
      std::lock_guard lock(mutex);view.cursor=receipt.cursor;view.completed=true;return {};
    }
    while(!stopping.load()){
      bool readable=false;const auto activity=connection.wait_activity(60000,readable);
      if(activity==TransportCode::timeout||(activity==TransportCode::ok&&!readable))continue;
      if(activity!=TransportCode::ok)return transport(activity);
      Bytes wire;const auto incoming=read(connection,wire,options.io_timeout_ms);
      if(!incoming)return incoming;
      std::vector<std::string> control;if(unpack(wire,control,32,65536)&&!control.empty()&&control[0]=="error")return remote_error(control);
      Delivery page;if(auto result=decode_delivery(wire,page,options.service);!result)return result;
      if(page.subscription_id!=sub.id||page.generation!=sub.generation||lifecycle::SourceRef{page.source.source_uid,page.source.epoch}!=sub.scope)return fail(Code::unauthorized,"received unexpected subscription or source");
      const auto committed=execute(executor,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.receive(session,page,now(),receipt);});
      if(!committed.committed())return committed;
      {std::lock_guard lock(mutex);++view.committed_pages;view.committed_bytes+=wire.size();view.cursor=receipt.cursor;}
      if(auto result=write(connection,pack({"ack",number(page.generation),number(page.to.sequence),page.to.payload_sha256}),options.io_timeout_ms);!result)return result;
      std::vector<std::string> ack;if(auto result=fields(connection,ack,options.io_timeout_ms);!result)return result;
      if(ack!=std::vector<std::string>{"acknowledged",number(page.to.sequence),page.to.payload_sha256})return fail(Code::corrupt,"server did not durably acknowledge exact cursor");
      if(options.through_sequence&&page.to.sequence>=options.through_sequence){std::lock_guard lock(mutex);view.completed=true;return {};}
    }
    return fail(Code::cancelled,"subscriber stopped");
  }
  void loop()noexcept {
    auto retry=options.minimum_retry_ms;
    while(!stopping.load()){
      Result result;try{result=run_connection();}catch(...){result=fail(Code::io_error,"subscriber exception");}
      connection.close();
      {std::lock_guard lock(mutex);view.result=result;if(view.completed)break;view.blocked=permanently_blocked(result.code);if(view.blocked||stopping.load())break;++view.reconnects;}
      std::unique_lock lock(mutex);changed.wait_for(lock,std::chrono::milliseconds(retry),[&]{return stopping.load();});
      retry=std::min(options.maximum_retry_ms,retry>options.maximum_retry_ms/2?options.maximum_retry_ms:retry*2);
    }
    {std::lock_guard lock(mutex);view.running=false;}
    changed.notify_all();
  }
};
NativeSubscriber::NativeSubscriber(host::IStoreTaskExecutor& executor,NativeSubscriberOptions options):impl_(std::make_unique<Impl>(executor,std::move(options))){}
NativeSubscriber::~NativeSubscriber(){stop();}
Result NativeSubscriber::start(){
  auto& value=*impl_;if(value.worker.joinable())return fail(Code::busy,"subscriber already started");
  if(!value.options.port||!valid_token(value.options.local_principal,256)||!valid_token(value.options.remote_principal,256)||
      !valid_digest(value.options.server_certificate_sha256)||!value.options.minimum_retry_ms||value.options.maximum_retry_ms<value.options.minimum_retry_ms||
      value.options.maximum_retry_ms>60000)return fail(Code::invalid,"invalid native subscriber profile");
  if(auto result=transport(value.context.open(credentials(value.options.credentials),tls::Role::client));!result)return result;
  value.stopping.store(false);value.view={};value.view.running=true;
  try{value.worker=std::thread([&value]{value.loop();});}catch(...){value.view.running=false;return fail(Code::io_error,"subscriber thread creation failed");}
  return {};
}
NativeSubscriberStatus NativeSubscriber::status()const{std::lock_guard lock(impl_->mutex);return impl_->view;}
Result NativeSubscriber::wait(std::uint32_t timeout_ms)const {
  std::unique_lock lock(impl_->mutex);
  if(!impl_->changed.wait_for(lock,std::chrono::milliseconds(timeout_ms),[&]{return !impl_->view.running;}))return fail(Code::deadline,"subscriber still running");
  return impl_->view.result;
}
void NativeSubscriber::request_stop()noexcept{impl_->stopping.store(true);impl_->connection.cancel();impl_->changed.notify_all();}
void NativeSubscriber::stop()noexcept{request_stop();if(impl_->worker.joinable())impl_->worker.join();}

Result fetch_native_artifact(host::IStoreTaskExecutor& executor,const NativeSubscriberOptions& options,std::string_view digest,ReceiveReceipt& receipt){
  tls::Context context;tls::Connection connection;if(auto result=transport(context.open(credentials(options.credentials),tls::Role::client));!result)return result;
  Session session;if(auto result=hello(connection,context,options,session,executor);!result)return result;
  Bytes object;std::uint64_t total=0;
  do {
    if(auto result=write(connection,pack({"artifact",options.subscription.scope.uid,number(options.subscription.scope.epoch),std::string(digest),number(object.size())}),options.io_timeout_ms);!result)return result;
    std::vector<std::string> chunk;if(auto result=fields(connection,chunk,options.io_timeout_ms);!result)return result;
    std::uint64_t offset=0,size=0;if(chunk.size()!=5||chunk[0]!="artifact"||chunk[1]!=digest||!integer(chunk[2],offset)||!integer(chunk[3],size)||
      offset!=object.size()||offset>size||size>options.service.maximum_artifact_bytes||(total&&size!=total)||chunk[4].size()>size-offset||(chunk[4].empty()&&offset<size))
      return fail(Code::corrupt,"artifact chunk continuity or size mismatch");
    total=size;object+=chunk[4];
  }while(object.size()<total);
  return execute(executor,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.receive_artifact(session,options.subscription.scope,digest,std::move(object),now(),receipt);});
}
Result install_native_bootstrap(host::IStoreTaskExecutor& executor,const NativeSubscriberOptions& options,std::uint64_t generation,ReceiveReceipt& receipt){
  tls::Context context;tls::Connection connection;if(auto result=transport(context.open(credentials(options.credentials),tls::Role::client));!result)return result;
  Session session;if(auto result=hello(connection,context,options,session,executor);!result)return result;
  ReceiveReceipt local;
  const auto received=execute(executor,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.received(session.principal,options.subscription.id,options.subscription.scope,local);});
  if(received&&local.generation==generation){
    if(auto result=write(connection,pack({"bootstrap-status",options.subscription.id,number(generation),local.receipt_sha256,number(local.cursor.sequence),local.cursor.payload_sha256}),options.io_timeout_ms);!result)return result;
    std::vector<std::string> accepted;const auto confirmed=fields(connection,accepted,options.io_timeout_ms);
    if(confirmed){
      if(accepted!=std::vector<std::string>{"bootstrap-accepted",options.subscription.id,number(generation),local.receipt_sha256})return fail(Code::corrupt,"bootstrap reconciliation identity mismatch");
      receipt=local;receipt.duplicate=true;return {};
    }
    if(confirmed.code!=Code::not_found)return confirmed;
    // A failed status query closes that bounded connection. Reconnect before
    // retrying the exact pending offer; never reinterpret removed history.
    if(auto result=hello(connection,context,options,session,executor);!result)return result;
  }else if(!received&&received.code!=Code::not_found)return received;
  if(auto result=write(connection,pack({"bootstrap",options.subscription.id,number(generation),options.subscription.scope.uid,number(options.subscription.scope.epoch),number(options.subscription.lease_until_unix_s)}),options.io_timeout_ms);!result)return result;
  std::vector<std::string> announced;if(auto result=fields(connection,announced,options.io_timeout_ms);!result)return result;
  std::uint64_t size=0;if(announced.size()!=3||announced[0]!="bootstrap"||!valid_digest(announced[1])||!integer(announced[2],size)||size>options.service.maximum_artifact_bytes+65536)
    return fail(Code::capacity,"invalid bootstrap transfer bounds");
  Bytes assembled;while(assembled.size()<size){
    if(auto result=write(connection,pack({"chunk",number(assembled.size())}),options.io_timeout_ms);!result)return result;
    std::vector<std::string> chunk;if(auto result=fields(connection,chunk,options.io_timeout_ms);!result)return result;
    if(chunk.size()!=3||chunk[0]!="chunk"||chunk[1]!=number(assembled.size())||chunk[2].empty()||chunk[2].size()>size-assembled.size())return fail(Code::corrupt,"bootstrap chunk continuity mismatch");
    assembled+=chunk[2];
  }
  if(sha256(assembled)!=announced[1])return fail(Code::corrupt,"bootstrap transfer digest mismatch");
  BootstrapOffer offer;if(auto result=decode_bootstrap(assembled,offer,options.service.maximum_artifact_bytes+65536);!result)return result;
  if(offer.subscription_id!=options.subscription.id||offer.generation!=generation||
      lifecycle::SourceRef{offer.source.source_uid,offer.source.epoch}!=options.subscription.scope||offer.lease_until_unix_s!=options.subscription.lease_until_unix_s)
    return fail(Code::unauthorized,"bootstrap reply differs from requested subscription, generation, scope or lease");
  const auto installed=execute(executor,[&](evidence::ICommitStore& store){Service service(store,options.service);return service.install_bootstrap(session,offer,now(),receipt);});
  if(!installed.committed())return installed;
  if(auto result=write(connection,pack({"installed",offer.id,offer.checkpoint_sha256}),options.io_timeout_ms);!result)return result;
  std::vector<std::string> accepted;if(auto result=fields(connection,accepted,options.io_timeout_ms);!result)return result;
  return accepted==std::vector<std::string>{"bootstrap-accepted",offer.id}?Result{}:fail(Code::corrupt,"invalid bootstrap acceptance result");
}
}
