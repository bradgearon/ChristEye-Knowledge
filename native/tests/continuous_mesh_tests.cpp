#include "anaiero/sdk/native_mesh.hpp"
#include "anaiero/sdk/native_session.hpp"
#include "anaiero/sdk/engine_files.hpp"
#include "anaiero/infrastructure/native_tls.hpp"
#include "anaiero/infrastructure/native_child_process.hpp"
#include "anaiero/infrastructure/native_ipc.hpp"
#include "mesh_test_credentials.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <fstream>
#include <iostream>
#include <thread>
namespace sdk=anaiero::sdk;
namespace ev=sdk::evidence;
namespace lc=sdk::lifecycle;
namespace mesh=sdk::mesh;
namespace host=sdk::host;
namespace tls=anaiero::mesh::tls;
namespace legacy=anaiero::mesh;
namespace engines=anaiero::engines;
namespace fs=std::filesystem;
using namespace std::chrono_literals;
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " line "<<__LINE__<<'\n';std::abort();}}while(false)
static void require(const sdk::Result& result){if(!result){std::cerr<<sdk::code_name(result.code)<<": "<<result.message<<'\n';std::abort();}}
static std::uint64_t now(){return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());}
static sdk::Result execute(host::IStoreTaskExecutor& executor,host::StoreTask task){std::promise<sdk::Result> promise;auto future=promise.get_future();require(executor.submit(std::move(task),[&](sdk::Result result){promise.set_value(std::move(result));}));CHECK(future.wait_for(30s)==std::future_status::ready);return future.get();}
static mesh::NativeCredentials credentials(const tls::Configuration& value){return {value.certificate_file,value.private_key_file,value.authority_file};}
struct Fixture {
  std::unique_ptr<host::NativeSession> session;std::unique_ptr<host::NativeContinuousHost> worker;sdk::EngineIdentity identity;
  Fixture(const fs::path& repository,const fs::path& path,std::string suffix){
    sdk::EngineModuleCatalog catalog;require(engines::register_builtin_engines(catalog));require(catalog.freeze());sdk::SessionDefinition definition;
    require(engines::read_session_definition((repository/"examples/runtime/modular-native.cevsession").string(),catalog,definition));
    auto environment=definition.engines.back();environment.identity.source_uid+=suffix;identity=environment.identity;definition.engines={environment};
    {std::unique_ptr<ev::ICommitStore> store;require(ev::open_native_store(path.string(),{},store));sdk::ProjectionService projection;sdk::SessionService runtime(catalog,*store,projection);sdk::SessionRunner runner(runtime,*store);require(runner.initialize(definition));}
    session=std::make_unique<host::NativeSession>();require(session->open(path.string()));require(session->ensure_engines());host::Options options;options.initially_running=false;options.period_ms=1;
    worker=std::make_unique<host::NativeContinuousHost>(session->runtime(),session->runner(),session->store(),options);
  }
  void start(){require(worker->start());}
  void through(std::uint64_t count){std::uint64_t ticket=0;require(worker->enqueue({host::CommandKind::through,{},{},count},ticket));const auto deadline=std::chrono::steady_clock::now()+30s;auto status=worker->snapshot();while((status.completed_cycles<count||status.cycle_pending)&&std::chrono::steady_clock::now()<deadline)status=worker->snapshot(status.revision,1000);CHECK(status.completed_cycles==count&&!status.cycle_pending&&status.result.ok());}
  lc::Source source(){lc::Source result;require(execute(*worker,[&](ev::ICommitStore& store){lc::Service service(store);return service.source({identity.source_uid,identity.epoch},result);}));return result;}
  void enroll(std::string principal,std::string digest){require(session->lifecycle().upsert_peer({principal,std::move(digest),1,now()+3600,lc::PeerPhase::active},0,now(),"enroll-"+principal));}
  void grant(std::string principal,const sdk::EngineIdentity& source,bool sending,std::vector<sdk::Digest> artifacts={}){mesh::Service service(session->store());require(service.configure_grant({std::move(principal),source,1,sending,!sending,true,std::move(artifacts)},0,sending?"grant-send":"grant-receive"));}
  ~Fixture(){worker->shutdown();}
};
static void send(tls::Connection& connection,std::string_view value){CHECK(connection.write_frame(std::as_bytes(std::span(value.data(),value.size())),5000)==legacy::TransportCode::ok);}
static sdk::Bytes receive(tls::Connection& connection){std::vector<std::byte> frame;CHECK(connection.read_frame(frame,5000)==legacy::TransportCode::ok);return {reinterpret_cast<const char*>(frame.data()),frame.size()};}
static tls::detail::Socket raw_connect(std::uint16_t port){
  const auto socket=tls::detail::make_socket(AF_INET);CHECK(socket!=tls::detail::kInvalidSocket);sockaddr_in address{};address.sin_family=AF_INET;address.sin_port=htons(port);address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
  const auto result=::connect(socket,reinterpret_cast<const sockaddr*>(&address),sizeof(address));CHECK(result==0||tls::detail::would_block(tls::detail::socket_error()));return socket;
}
static void native_roundtrip(const fs::path& repository,const fs::path& root){
  legacy::test::Credentials keys;CHECK(legacy::test::create_credentials(root/"keys",keys));
  Fixture sender(repository,root/"sender","-sender"),receiver(repository,root/"receiver","-receiver");
  sender.enroll("receiver",keys.client_pin);receiver.enroll("sender",keys.server_pin);
  const std::string artifact(40000,'z');const auto artifact_digest=sdk::sha256(artifact);
  ev::CommitBatch batch;batch.operation_id="artifact";batch.request_digest=artifact_digest;batch.blobs.push_back({artifact_digest,artifact,{}});batch.records.push_back({"operator.artifact","fixture",artifact_digest,{artifact_digest},false});ev::CommitReceipt committed;require(sender.session->store().commit(batch,committed));
  sender.grant("receiver",sender.identity,true,{artifact_digest});receiver.grant("sender",sender.identity,false,{artifact_digest});
  mesh::NativeServerOptions options;options.credentials=credentials(keys.server);options.handshake_timeout_ms=10000;options.io_timeout_ms=60000;options.service.maximum_page_observations=2;
  mesh::NativeServer server(*sender.worker,options);sender.worker->set_commit_wakeup([&](std::uint64_t generation){server.notify_commit(generation);});sender.start();receiver.start();require(server.start());
  const auto port=server.status().port;CHECK(port);
  const auto raw=raw_connect(port);
  const auto wait=std::chrono::steady_clock::now()+2s;while(server.status().handshakes==0&&std::chrono::steady_clock::now()<wait)std::this_thread::sleep_for(5ms);CHECK(server.status().handshakes==1);
  tls::Context context;auto configured=keys.client;configured.application_protocol="anaiero-mesh/2";CHECK(context.open(configured,tls::Role::client)==legacy::TransportCode::ok);
  tls::Connection slow;const auto started=std::chrono::steady_clock::now();CHECK(slow.connect(context,{"127.0.0.1",port},keys.server_pin,3000)==legacy::TransportCode::ok);CHECK(std::chrono::steady_clock::now()-started<3s);
  CHECK(receive(slow)=="CEVMESH/2 READY\n");send(slow,sdk::pack({"hello","receiver"}));CHECK(receive(slow)==sdk::pack({"authenticated"}));
  tls::detail::close_socket(raw);
  tls::Context old_context;CHECK(old_context.open(keys.client,tls::Role::client)==legacy::TransportCode::ok);tls::Connection incompatible;
  CHECK(incompatible.connect(old_context,{"127.0.0.1",port},keys.server_pin,3000)!=legacy::TransportCode::ok);
  sender.through(2);const auto source=sender.source();CHECK(source.last_sequence>0);
  send(slow,sdk::pack({"subscribe","slow","1",source.id.uid,std::to_string(source.id.epoch),std::to_string(now()+300),"0",""}));
  std::vector<std::string> slow_sub;require(sdk::unpack(receive(slow),slow_sub));CHECK(slow_sub.size()==3&&slow_sub[0]=="subscribed");
  mesh::Delivery slow_page;require(mesh::decode_delivery(receive(slow),slow_page)); // Deliberately withhold ACK.
  mesh::NativeSubscriberOptions pull;pull.credentials=credentials(keys.client);pull.address="127.0.0.1";pull.port=port;pull.local_principal="receiver";pull.remote_principal="sender";pull.server_certificate_sha256=keys.server_pin;
  pull.subscription={"fast","receiver",source.id,1,now()+300,lc::SubscriptionPhase::active,{},{},false};pull.through_sequence=source.last_sequence;
  mesh::NativeSubscriber subscriber(*receiver.worker,pull);require(subscriber.start());require(subscriber.wait(20000));CHECK(subscriber.status().completed&&subscriber.status().cursor.sequence==source.last_sequence);subscriber.stop();
  // A slow same-principal connection owns only its one page; unrelated work
  // still finishes and sender ACK advances only to the exact committed cursor.
  lc::Subscription persisted;require(execute(*sender.worker,[&](ev::ICommitStore& store){lc::Service lifecycle(store);return lifecycle.subscription("fast",persisted);}));CHECK(persisted.acknowledged.sequence==source.last_sequence);
  CHECK(server.status().delivered_pages>=2&&server.status().acknowledged_pages>=1&&server.status().queued_work<=options.maximum_clients+options.maximum_handshakes);
  mesh::ReceiveReceipt receipt;require(mesh::fetch_native_artifact(*receiver.worker,pull,artifact_digest,receipt));
  CHECK(receipt.cursor.sequence==0&&receipt.cursor.payload_sha256.empty()&&receipt.generation==0&&receipt.store_generation>0&&!receipt.duplicate);
  const auto artifact_generation=receipt.store_generation;
  require(mesh::fetch_native_artifact(*receiver.worker,pull,artifact_digest,receipt));
  CHECK(receipt.cursor.sequence==0&&receipt.cursor.payload_sha256.empty()&&receipt.generation==0&&receipt.store_generation==artifact_generation&&receipt.duplicate);
  sdk::Bytes object;require(execute(*receiver.worker,[&](ev::ICommitStore& store){return store.get_blob(artifact_digest,object);}));CHECK(object==artifact);
  // New bootstrap generation installs an exact checkpoint plus cursor. It is
  // acknowledged as installation, never as delivery of missing observations.
  require(mesh::install_native_bootstrap(*receiver.worker,pull,2,receipt));CHECK(receipt.cursor.sequence==source.last_sequence);
  require(mesh::install_native_bootstrap(*receiver.worker,pull,2,receipt));CHECK(receipt.duplicate); // Lost final acceptance reply reconciles.
  sender.through(3);const auto advanced=sender.source();pull.subscription.generation=2;pull.through_sequence=advanced.last_sequence;
  mesh::NativeSubscriber continued(*receiver.worker,pull);require(continued.start());require(continued.wait(20000));CHECK(continued.status().cursor.sequence==advanced.last_sequence);continued.stop();
  // Local administration reaches the same canonical lane and reclaims only
  // the explicitly reviewed received artifact. TLS exposes no such verb.
  sdk::Bytes retention;
  require(mesh::administer_native(*receiver.worker,"receiver-retention-review tls-artifact sender "+source.id.uid+' '+std::to_string(source.id.epoch)+" 0 0 review-tls-artifact artifact:"+artifact_digest,retention));
  CHECK(retention.find("reviewed GENERATION")!=std::string::npos);
  require(mesh::administer_native(*receiver.worker,"receiver-retention-status tls-artifact",retention));CHECK(retention.find("reviewed GENERATION")!=std::string::npos);
  require(mesh::administer_native(*receiver.worker,"receiver-retention-activate tls-artifact activate-tls-artifact",retention));
  require(mesh::administer_native(*receiver.worker,"receiver-retention-reclaim tls-artifact reclaim-tls-artifact",retention));
  require(mesh::administer_native(*receiver.worker,"receiver-retention-status tls-artifact",retention));CHECK(retention.find("completed GENERATION")!=std::string::npos);
  CHECK(mesh::fetch_native_artifact(*receiver.worker,pull,artifact_digest,receipt).code==sdk::Code::history_expired);
  pull.through_sequence=0;mesh::NativeSubscriber waiting(*receiver.worker,pull);require(waiting.start());
  const auto connected_until=std::chrono::steady_clock::now()+5s;while(waiting.status().connections==0&&std::chrono::steady_clock::now()<connected_until)std::this_thread::sleep_for(5ms);CHECK(waiting.status().connections==1);
  std::this_thread::sleep_for(100ms);const auto quiet_wakeups=server.status().owner_wakeups;
  std::this_thread::sleep_for(150ms);CHECK(server.status().owner_wakeups==quiet_wakeups); // Caught-up peers do not poll canonical state.
  sdk::Bytes admin;require(mesh::administer_native(*sender.worker,"peer receiver "+keys.client_pin+" 2 1 "+std::to_string(now()+3600)+" revoked live-revoke",admin));
  const auto revoked=waiting.wait(10000);CHECK(revoked.code==sdk::Code::unauthorized||revoked.code==sdk::Code::expired);CHECK(waiting.status().blocked);waiting.stop();
  send(slow,sdk::pack({"ack",std::to_string(slow_page.generation),std::to_string(slow_page.to.sequence),slow_page.to.payload_sha256}));
  std::vector<std::string> denied;require(sdk::unpack(receive(slow),denied));CHECK(denied.size()==3&&denied[0]=="error");slow.close();
  server.stop();sender.worker->shutdown();receiver.worker->shutdown();ev::AuditReport audit;require(sender.session->store().audit(audit));require(receiver.session->store().audit(audit));
}
static std::uint64_t number_field(std::string_view text,std::string_view key){
  const auto pattern="\""+std::string(key)+"\":";const auto offset=text.find(pattern);CHECK(offset!=std::string_view::npos);std::uint64_t value=0;
  const auto converted=std::from_chars(text.data()+offset+pattern.size(),text.data()+text.size(),value);CHECK(converted.ec==std::errc{});return value;
}
static std::string ipc(std::string_view endpoint,std::string_view command){std::string response;CHECK(anaiero::host::NativeIpcClient::request(endpoint,command,response,5000));return response;}
static std::string admin(std::string_view endpoint,std::string_view command,bool mutation=true){
  const auto queued=ipc(endpoint,"MESH "+std::string(command));CHECK(queued.find("\"queued\":true")!=std::string::npos&&queued.find("\"durable\":false")!=std::string::npos);
  const auto ticket=number_field(queued,"ticket");CHECK(ipc(endpoint,"PING")=="PONG\n");
  const auto until=std::chrono::steady_clock::now()+15s;std::string result;
  do {result=ipc(endpoint,"MESH_RESULT "+std::to_string(ticket));if(result.find("\"complete\":true")!=std::string::npos)break;std::this_thread::sleep_for(5ms);}while(std::chrono::steady_clock::now()<until);
  CHECK(result.find("\"complete\":true")!=std::string::npos&&result.find("\"code\":\"ok\"")!=std::string::npos&&result.find(mutation?"\"durable\":true":"\"durable\":false")!=std::string::npos);return result;
}
static std::string hex(std::string_view value){static constexpr char digits[]="0123456789abcdef";std::string out;for(unsigned char c:value){out+=digits[c>>4];out+=digits[c&15];}return out;}
static void startup_diagnostics(const std::string& executable){
  anaiero::process::NativeChildProcess child;CHECK(child.start({executable,{"invalid-command"}})==anaiero::process::Code::ok);
  const auto deadline=std::chrono::steady_clock::now()+5s;
  do {child.collect_diagnostics();if(!child.diagnostics().empty())break;std::this_thread::sleep_for(5ms);}while(std::chrono::steady_clock::now()<deadline);
  CHECK(child.diagnostics().find("unknown host command")!=std::string::npos);child.stop();
}
static void native_cli(const fs::path& repository,const fs::path& root,const std::string& executable){
  legacy::test::Credentials keys;CHECK(legacy::test::create_credentials(root/"cli-keys",keys));sdk::EngineIdentity source;
  {Fixture initialize(repository,root/"cli-sender","-cli-sender");source=initialize.identity;}
  Fixture receiver(repository,root/"cli-receiver","-cli-receiver");receiver.enroll("sender",keys.server_pin);receiver.grant("sender",source,false);receiver.start();
  const auto config=root/"server.mesh";{std::ofstream file(config);file<<"ANAIEROMESH/2\ncertificate="<<keys.server.certificate_file<<"\nprivate_key="<<keys.server.private_key_file<<"\nauthority="<<keys.server.authority_file<<"\naddress=127.0.0.1\nport=0\n";CHECK(file.good());}
#ifdef _WIN32
  const std::string endpoint="anaiero-mesh-cli-"+std::to_string(GetCurrentProcessId());
#else
  // Checkout/build paths can exceed the native Unix endpoint limit. Keep the
  // control socket in its own short, private temporary directory.
  std::string endpoint_directory=(fs::temp_directory_path()/"anaiero-mesh-XXXXXX").string();
  CHECK(::mkdtemp(endpoint_directory.data()));
  const std::string endpoint=(fs::path(endpoint_directory)/"control.sock").string();
  CHECK(anaiero::host::valid_endpoint(endpoint,false));
#endif
  anaiero::process::NativeChildProcess child;CHECK(child.start({executable,{"start",endpoint,(root/"cli-sender").string(),"10","paused","--mesh",config.string()}})==anaiero::process::Code::ok);
  std::string pong;const auto ready=std::chrono::steady_clock::now()+20s;
  while(!anaiero::host::NativeIpcClient::request(endpoint,"PING",pong,100)&&std::chrono::steady_clock::now()<ready)std::this_thread::sleep_for(10ms);
  if(pong!="PONG\n"){child.collect_diagnostics();std::cerr<<"host child diagnostics: "<<child.diagnostics()<<'\n';}
  CHECK(pong=="PONG\n");
  const auto scope=admin(endpoint,"scope "+source.source_uid+" "+std::to_string(source.epoch),false);CHECK(scope.find(hex(sdk::encode_identity(source)))!=std::string::npos);
  const auto expiry=std::to_string(now()+3600);(void)admin(endpoint,"peer receiver "+keys.client_pin+" 1 0 "+expiry+" active cli-enroll");
  (void)admin(endpoint,"grant receiver "+hex(sdk::encode_identity(source))+" 1 0 send bootstrap cli-grant");
  CHECK(ipc(endpoint,"THROUGH 2").find("\"queued\":true")!=std::string::npos);
  const auto completed=std::chrono::steady_clock::now()+20s;std::string status;
  do {status=ipc(endpoint,"STATUS");if(number_field(status,"completed_cycles")==2)break;std::this_thread::sleep_for(10ms);}while(std::chrono::steady_clock::now()<completed);CHECK(number_field(status,"completed_cycles")==2);
  const auto port=number_field(ipc(endpoint,"MESH_STATUS"),"port");CHECK(port>0&&port<=65535);
  mesh::NativeSubscriberOptions options;options.credentials=credentials(keys.client);options.port=static_cast<std::uint16_t>(port);options.local_principal="receiver";options.remote_principal="sender";options.server_certificate_sha256=keys.server_pin;
  options.subscription={"cli-sub","receiver",{source.source_uid,source.epoch},1,now()+300,lc::SubscriptionPhase::active,{},{},false};
  mesh::NativeSubscriber subscriber(*receiver.worker,options);require(subscriber.start());
  const auto delivered=std::chrono::steady_clock::now()+20s;while(subscriber.status().committed_pages==0&&std::chrono::steady_clock::now()<delivered)std::this_thread::sleep_for(5ms);CHECK(subscriber.status().committed_pages>0);
  (void)admin(endpoint,"peer receiver "+keys.client_pin+" 2 1 "+expiry+" revoked cli-revoke");
  const auto blocked=subscriber.wait(10000);CHECK(blocked.code==sdk::Code::unauthorized||blocked.code==sdk::Code::expired);subscriber.stop();
  CHECK(ipc(endpoint,"MESH_STATUS").find("\"acknowledged_pages\":")!=std::string::npos);
  CHECK(ipc(endpoint,"STOP").starts_with("BYE"));
  std::unique_ptr<host::NativeSession> reopened;const auto stopped=std::chrono::steady_clock::now()+15s;sdk::Result open;
  do {reopened=std::make_unique<host::NativeSession>();open=reopened->open((root/"cli-sender").string());if(open)break;reopened.reset();std::this_thread::sleep_for(10ms);}while(std::chrono::steady_clock::now()<stopped);
  require(open);ev::AuditReport audit;require(reopened->store().audit(audit));CHECK(reopened->runner().progress().completed_cycles==2);child.stop();
#ifndef _WIN32
  fs::remove_all(endpoint_directory);
#endif
}
int main(int argc,char** argv){
  CHECK(argc==5);const std::string mode=argv[4];CHECK(mode=="transport"||mode=="host-cli");
#ifndef _WIN32
  if(mode=="host-cli"){
    const int probe=::socket(AF_UNIX,SOCK_STREAM,0);
    if(probe<0&&(errno==EPERM||errno==EACCES)){
      if(std::getenv("ANAIERO_REQUIRE_IPC")){std::cerr<<"Required native local IPC is unavailable\n";return 1;}
      std::cout<<"SKIP: environment denies native Unix-domain socket creation\n";return 77;
    }
    CHECK(probe>=0);::close(probe);
  }
#endif
  const fs::path root=argv[2];std::error_code error;fs::remove_all(root,error);CHECK(!error);fs::create_directories(root);
  if(mode=="transport"){startup_diagnostics(argv[3]);native_roundtrip(argv[1],root);}else native_cli(argv[1],root,argv[3]);
  std::cout<<"continuous native mesh "<<mode<<": canonical shared-host integration passed\n";
}
