// Production TLS/host/application/client integration, durable evidence,
// subscriptions, artifact receipts, revocation and complete process-state reopen.
#include "anaiero/infrastructure/native_mesh_host.hpp"
#include "anaiero/application/native_mesh_client.hpp"
#include "anaiero/infrastructure/native_child_process.hpp"
#include "mesh_test_credentials.hpp"
#include <array>
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <thread>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace mesh=anaiero::mesh;
namespace tls=mesh::tls;
namespace app=anaiero::application;
namespace runtime=anaiero::runtime;
namespace fs=std::filesystem;
using Code=mesh::TransportCode;
using namespace std::chrono_literals;
static constexpr mesh::Scope kForward{101,1,1,101},kReverse{201,1,1,201};
static bool tls_required() {
#ifdef _WIN32
  // An empty value still requires TLS; only absence permits a skip.
  ::SetLastError(ERROR_SUCCESS);
  return ::GetEnvironmentVariableA("ANAIERO_REQUIRE_MESH_TLS",nullptr,0)!=0||
    ::GetLastError()!=ERROR_ENVVAR_NOT_FOUND;
#else
  return std::getenv("ANAIERO_REQUIRE_MESH_TLS")!=nullptr;
#endif
}
static void check(bool success,std::string_view explanation){
  if(!success){std::cerr<<"FAILED integration: "<<explanation<<'\n';std::abort();}
}
static std::span<const std::byte> bytes(std::string_view value){return std::as_bytes(std::span(value.data(),value.size()));}
static std::string text(const std::vector<std::byte>& value){return {reinterpret_cast<const char*>(value.data()),value.size()};}
static void write(const fs::path& path,std::string_view value){
  CHECK(!value.empty());std::ofstream file(path,std::ios::binary|std::ios::trunc);CHECK(file);
  file.write(value.data(),static_cast<std::streamsize>(value.size()));file.close();CHECK(file);
}
static std::uint64_t field(std::string_view json,std::string_view name){
  const std::string key='"'+std::string(name)+"\":";const auto offset=json.find(key);CHECK(offset!=std::string_view::npos);
  std::uint64_t result=0;const auto parsed=std::from_chars(json.data()+offset+key.size(),json.data()+json.size(),result);
  CHECK(parsed.ec==std::errc{});return result;
}
static std::string administer(mesh::NativeMeshHost& host,std::string_view command){
  const auto reply=host.administration().handle(command);return {reply.bytes.data(),reply.length};
}
static std::string tls_exchange(tls::Connection& peer,std::string_view request){
  CHECK(peer.write_frame(bytes(request),5000)==Code::ok);
  std::vector<std::byte> reply;CHECK(peer.read_frame(reply,5000)==Code::ok);return text(reply);
}
static void connect(tls::Connection& peer,tls::Context& context,const mesh::Endpoint& endpoint,std::string_view pin){
  CHECK(peer.connect(context,endpoint,pin,5000)==Code::ok);
  std::vector<std::byte> reply;CHECK(peer.read_frame(reply,5000)==Code::ok&&text(reply)=="CEVMESH/1 READY\n");
}
static runtime::Observation observation(mesh::Scope scope,std::uint64_t sequence){
  runtime::Observation value{};value.source=scope.source;value.source_epoch=scope.epoch;
  value.clock_domain=scope.clock;value.colony=scope.colony;value.animon=scope.source;
  value.metric=runtime::Metric::activity;value.lane=runtime::Lane::health;
  value.sequence=sequence;value.acquired_ns=sequence*1000;value.value=static_cast<std::int32_t>(sequence%1000);
  return value;
}
static mesh::Enrollment enrollment(std::string name,std::string pin,std::uint64_t expiry){
  mesh::Enrollment value;value.principal=std::move(name);value.certificate_sha256=std::move(pin);
  value.generation=1;value.expires_unix_s=expiry;return value;
}
static void publication(mesh::Enrollment& entry,mesh::Scope scope){
  entry.publications[entry.publication_count++]=scope;entry.capabilities|=mesh::capability_bit(mesh::MeshCapability::publish);
}
static void subscription(mesh::Enrollment& entry,mesh::Scope scope){
  entry.subscriptions[entry.subscription_count++]=scope;entry.capabilities|=mesh::capability_bit(mesh::MeshCapability::subscribe);
}
static void install(app::NativeMeshApplication& application,const mesh::Enrollment& entry){
  CHECK(application.authorization().upsert(entry,application.authorization().snapshot().revision)==mesh::EnrollmentCode::ok);
}
static void seed(app::NativeMeshApplication& application,tls::Context& context,mesh::Scope scope,
                 std::uint64_t first,std::uint64_t last){
  app::LocalMeshIdentity identity(context);mesh::VerifiedMeshSession session;
  CHECK(application.authorization().authenticate(identity,session)==mesh::EnrollmentCode::ok);
  for(auto i=first;i<=last;++i){const auto received=application.receive(session,observation(scope,i),mesh::IngressKind::local_import);
    CHECK(received.code==mesh::ReplayCode::ok&&!received.duplicate&&received.receipt.kind==mesh::IngressKind::local_import);}
}
static void configuration(const fs::path& path,std::string name,std::string principal,
                          const tls::Configuration& credentials,std::uint16_t port,std::string_view pin={}){
  mesh::files::MeshConfiguration config;config.name=std::move(name);config.principal=std::move(principal);
  config.certificate_file=credentials.certificate_file;config.private_key_file=credentials.private_key_file;
  config.ca_file=credentials.authority_file;config.deadline_ms=5000;
  if(pin.empty())config.listen_port=port?port:1; // in-process tests explicitly request an ephemeral listener
  else {config.connect_port=port;config.server_pin=pin;}
  write(path,mesh::files::serialize_configuration(config));
}
static std::unique_ptr<app::NativeMeshApplication> application(const fs::path& config,const fs::path& state){
  auto value=std::make_unique<app::NativeMeshApplication>();check(value->open(config,state),value->error());return value;
}
static std::unique_ptr<mesh::NativeMeshHost> serve(app::NativeMeshApplication& application,tls::Context& context){
  auto host=std::make_unique<mesh::NativeMeshHost>(application,context,mesh::Endpoint{"127.0.0.1",0});
  CHECK(host->start()==Code::ok);return host;
}
static void verify_artifact(app::NativeMeshApplication& application,const mesh::ArtifactReceipt& receipt,std::string_view expected){
  bool found=false;CHECK(application.artifact_store().completed(receipt,found)==mesh::ArtifactCode::ok&&found);
  std::array<std::byte,mesh::kMeshArtifactChunkBytes> buffer{};std::string received;
  for(std::uint64_t offset=0;offset<receipt.bytes;){std::size_t count=0;
    CHECK(application.artifact_store().read(receipt.sha256,offset,buffer,count)==mesh::ArtifactCode::ok&&count);
    received.append(reinterpret_cast<const char*>(buffer.data()),count);offset+=count;}
  CHECK(received==expected&&christeye::living::plans::sha256_hex(received)==receipt.sha256);
}
static void production_roundtrip(const fs::path& root,const mesh::test::Credentials& credentials,
                                tls::Context& server_context,tls::Context& client_context){
  const auto server_config=root/"server.cevmesh",client_config=root/"client.cevmesh";
  const auto server_state=root/"server-state",client_state=root/"client-state";
  configuration(server_config,"mesh-server","server",credentials.server,0);
  auto server=application(server_config,server_state);
  std::string artifact(mesh::kMeshArtifactChunkBytes*5+713,'\0');
  for(std::size_t i=0;i<artifact.size();++i)artifact[i]=static_cast<char>(i%251);
  const mesh::ArtifactReceipt artifact_receipt{christeye::living::plans::sha256_hex(artifact),artifact.size()};
  const std::string secret="ungranted artifact: knowledge of its digest grants no read access";
  const mesh::ArtifactReceipt secret_receipt{christeye::living::plans::sha256_hex(secret),secret.size()};
  CHECK(server->artifact_store().commit(artifact_receipt,artifact)==mesh::ArtifactCode::ok);
  CHECK(server->artifact_store().commit(secret_receipt,secret)==mesh::ArtifactCode::ok);
  const auto expiry=server_context.certificate_expiry_unix_seconds()-10;
  auto self=enrollment("server",credentials.server_pin,expiry);publication(self,kForward);install(*server,self);
  auto client_grant=enrollment("client",credentials.client_pin,expiry);publication(client_grant,kReverse);subscription(client_grant,kForward);
  client_grant.artifact_sha256[client_grant.artifact_count++]=artifact_receipt.sha256;
  client_grant.capabilities|=mesh::capability_bit(mesh::MeshCapability::artifact_read);install(*server,client_grant);
  seed(*server,server_context,kForward,1,80);CHECK(server->snapshot().committed_updates==80);
  auto host=serve(*server,server_context);
  configuration(client_config,"mesh-client","client",credentials.client,host->endpoint().port,credentials.server_pin);
  auto client=application(client_config,client_state);
  auto remote=enrollment("server",credentials.server_pin,expiry);publication(remote,kForward);subscription(remote,kReverse);install(*client,remote);
  auto local=enrollment("client",credentials.client_pin,expiry);publication(local,kReverse);install(*client,local);
  std::string original_receipt;
  {
    app::NativeMeshClient link(*client);check(link.connect(),link.error());app::MeshTransferStats stats;
    check(link.synchronize("primary",kForward,stats),link.error());CHECK(stats.records==80&&stats.duplicates==0);
    CHECK(client->snapshot().committed_updates==80);
    const auto first=client->receive(link.remote(),observation(kForward,1));
    CHECK(first.code==mesh::ReplayCode::ok&&first.duplicate&&first.receipt.kind==mesh::IngressKind::mutual_tls);
    CHECK(first.receipt.certificate_sha256==credentials.server_pin);original_receipt=first.receipt.receipt_sha256;
    stats={};check(link.fetch_artifact(artifact_receipt.sha256,stats),link.error());CHECK(stats.bytes==artifact.size());
    verify_artifact(*client,artifact_receipt,artifact);
    stats={};CHECK(!link.fetch_artifact(secret_receipt.sha256,stats));
    bool leaked=false;CHECK(client->artifact_store().completed(secret_receipt,leaked)==mesh::ArtifactCode::ok&&!leaked);
  }
  host->stop();host.reset();CHECK(server->snapshot().committed_updates==80);client.reset();
  seed(*server,server_context,kForward,81,96);
  host=serve(*server,server_context);
  configuration(client_config,"mesh-client","client",credentials.client,host->endpoint().port,credentials.server_pin);
  client=application(client_config,client_state);CHECK(client->snapshot().committed_updates==80);
  verify_artifact(*client,artifact_receipt,artifact);
  {
    app::NativeMeshClient link(*client);check(link.connect(),link.error());app::MeshTransferStats stats;
    check(link.synchronize("primary",kForward,stats),link.error());CHECK(stats.records==16&&stats.duplicates==0);
    CHECK(client->snapshot().committed_updates==96);
    stats={};check(link.synchronize("primary",kForward,stats),link.error());CHECK(stats.records==0);
    seed(*client,client_context,kReverse,1,7);
    stats={};check(link.push("return",kReverse,stats),link.error());CHECK(stats.records==7&&stats.duplicates==0);
    stats={};check(link.push("return",kReverse,stats),link.error());CHECK(stats.records==0);
    CHECK(field(administer(*host,"STATE"),"projected_observations")==103);
  }
  host->stop();host.reset();CHECK(server->snapshot().committed_updates==103);client.reset();
  seed(*server,server_context,kForward,97,112);
  host=serve(*server,server_context);
  configuration(client_config,"mesh-client","client",credentials.client,host->endpoint().port,credentials.server_pin);
  client=application(client_config,client_state);CHECK(client->snapshot().committed_updates==103);
  mesh::ReplayCursor unacknowledged;
  { // Durable local evidence + cursor exist, but no sender ACK survives this connection.
    tls::Connection peer;connect(peer,client_context,host->endpoint(),credentials.server_pin);
    mesh::VerifiedMeshSession remote_session;CHECK(client->authorization().authenticate(peer.peer(),remote_session)==mesh::EnrollmentCode::ok);
    mesh::wire::Request request;request.command=mesh::wire::Command::subscribe;request.id="primary";request.scope=kForward;
    CHECK(client->replay_store().load_receive(remote_session.authorized_peer(),"primary",kForward,request.cursor)==mesh::ReplayCode::ok);
    const auto actual_tip=request.cursor;request.cursor.hash[0]=request.cursor.hash[0]=='a'?'b':'a';
    CHECK(tls_exchange(peer,mesh::wire::encode(request))=="CEVMESH/1 ERROR receive_tip_conflict\n");
    request.cursor=actual_tip;
    mesh::ReplayCursor cursor;CHECK(mesh::wire::parse_cursor_reply(tls_exchange(peer,mesh::wire::encode(request)),"SUBSCRIBED",cursor));
    CHECK(cursor.sequence==96);request.command=mesh::wire::Command::next;
    mesh::ReplayPage page;CHECK(mesh::wire::parse_page(tls_exchange(peer,mesh::wire::encode(request)),page));
    CHECK(page.count==16&&page.caught_up&&page.terminal.sequence==119);
    for(std::size_t i=0;i<page.count;++i){const auto& record=page.records[i];
      const auto accepted=client->receive(remote_session,record.observation);CHECK(accepted.code==mesh::ReplayCode::ok&&!accepted.duplicate);
      CHECK(client->replay_store().save_receive(remote_session.authorized_peer(),"primary",kForward,record.cursor)==mesh::ReplayCode::ok);}
    unacknowledged=page.terminal;CHECK(client->snapshot().committed_updates==119);
    // Also verify malformed protocol data and forged clock do not publish evidence.
    CHECK(tls_exchange(peer,"CEVMESH/1 PING extra\n")=="CEVMESH/1 ERROR malformed\n");
    CHECK(tls_exchange(peer,"CEVMESH/1 PUBLISH 00\n")=="CEVMESH/1 ERROR malformed\n");
    request.command=mesh::wire::Command::publish;request.observation=observation(kReverse,8);request.observation.clock_domain=2;
    CHECK(tls_exchange(peer,mesh::wire::encode(request)).starts_with("CEVMESH/1 ERROR "));
    CHECK(field(administer(*host,"STATE"),"projected_observations")==119);
    peer.close();
  }
  host->stop();host.reset();client.reset();server.reset();
  server=application(server_config,server_state);CHECK(server->snapshot().committed_updates==119);
  host=serve(*server,server_context);
  configuration(client_config,"mesh-client","client",credentials.client,host->endpoint().port,credentials.server_pin);
  client=application(client_config,client_state);CHECK(client->snapshot().committed_updates==119);
  {
    app::NativeMeshClient link(*client);check(link.connect(),link.error());app::MeshTransferStats stats;
    mesh::ReplayCursor recovered;
    CHECK(client->replay_store().load_receive(link.remote().authorized_peer(),"primary",kForward,recovered)==mesh::ReplayCode::ok&&recovered==unacknowledged);
    check(link.synchronize("primary",kForward,stats),link.error());CHECK(stats.records==16&&stats.duplicates==16);
    CHECK(client->snapshot().committed_updates==119);
    const auto first=client->receive(link.remote(),observation(kForward,1));
    CHECK(first.code==mesh::ReplayCode::ok&&first.duplicate&&first.receipt.receipt_sha256==original_receipt);
    stats={};check(link.synchronize("primary",kForward,stats),link.error());CHECK(stats.records==0);
  }
  host->stop();host.reset();
  auto rotated_policy=server->authorization().snapshot();CHECK(rotated_policy.revision==2);
  host=serve(*server,server_context);
  {
    tls::Connection stale;connect(stale,client_context,host->endpoint(),credentials.server_pin);
    CHECK(administer(*host,"REVOKE client 2")=="OK revoked\n");
    CHECK(tls_exchange(stale,"CEVMESH/1 PING\n")=="CEVMESH/1 ERROR denied\n");
    std::vector<std::byte> reply;CHECK(stale.read_frame(reply,1000)!=Code::ok);
    tls::Connection revoked;CHECK(revoked.connect(client_context,host->endpoint(),credentials.server_pin,5000)==Code::ok);
    CHECK(revoked.read_frame(reply,5000)!=Code::ok);
    // Revocation advanced generation 1 -> 2; rotation advances it to 3.
    rotated_policy.revision=4;rotated_policy.peers[1].generation=3;rotated_policy.peers[1].revoked=false;
    rotated_policy.peers[1].certificate_sha256=credentials.rotated_client_pin;
    const auto policy_file=root/"rotated.cevpeers";write(policy_file,mesh::files::serialize_policy(rotated_policy));
    CHECK(administer(*host,"IMPORT \""+policy_file.string()+"\" 3")=="OK imported\n");
    tls::Connection old;CHECK(old.connect(client_context,host->endpoint(),credentials.server_pin,5000)==Code::ok);
    CHECK(old.read_frame(reply,5000)!=Code::ok);
    tls::Context rotated;CHECK(rotated.open(credentials.rotated_client,tls::Role::client)==Code::ok);
    tls::Connection fresh;connect(fresh,rotated,host->endpoint(),credentials.server_pin);
    CHECK(tls_exchange(fresh,"CEVMESH/1 PING\n")=="CEVMESH/1 PONG\n");
  }
  host->stop();host.reset();
  CHECK(client->authorization().rotate("client",credentials.rotated_client_pin,expiry,
    client->authorization().snapshot().revision)==mesh::EnrollmentCode::ok);client.reset();
  host=serve(*server,server_context);
  configuration(client_config,"mesh-client","client",credentials.rotated_client,host->endpoint().port,credentials.server_pin);
  client=application(client_config,client_state);
  {
    app::NativeMeshClient link(*client);check(link.connect(),link.error());app::MeshTransferStats stats;
    check(link.synchronize("primary",kForward,stats),link.error());CHECK(stats.records==0);
    stats={};check(link.push("return",kReverse,stats),link.error());CHECK(stats.records==0);
    stats={};check(link.fetch_artifact(artifact_receipt.sha256,stats),link.error());CHECK(stats.bytes==artifact.size());
    verify_artifact(*client,artifact_receipt,artifact);
  }
  host->stop();host.reset();CHECK(server->snapshot().committed_updates==119&&client->snapshot().committed_updates==119);
  CHECK(server->maintain()&&client->maintain());client.reset();server.reset();
  server=application(server_config,server_state);CHECK(server->snapshot().committed_updates==119);
  CHECK(server->authorization().snapshot()==rotated_policy);
  verify_artifact(*server,artifact_receipt,artifact);
}

static void parser_regressions(){
  mesh::ReplayPage page;page.count=1;page.examined=1;page.caught_up=true;
  page.terminal={1,std::string(64,'a')};page.records[0]={{1,std::string(64,'b')},observation(kForward,1)};
  mesh::ReplayPage decoded;CHECK(!mesh::wire::parse_page(mesh::wire::page(page),decoded));
  page={};CHECK(!mesh::wire::parse_page(mesh::wire::page(page),decoded));
  mesh::ArtifactChunk chunk;chunk.artifact={std::string(64,'a'),100};
  mesh::ArtifactChunk parsed;CHECK(!mesh::wire::parse_chunk(mesh::wire::chunk(chunk),parsed));
}
static void missing_receipt_recovery(const fs::path& directory,const mesh::test::Credentials& credentials,
                                     tls::Context& context){
  const auto root=directory/"receipt-rollback",config=root/"node.cevmesh",state=root/"state",backup=root/"metadata-before-receipts";
  std::error_code error;fs::create_directories(root,error);CHECK(!error);
  configuration(config,"receipt-rollback","server",credentials.server,0);
  auto node=application(config,state);
  auto self=enrollment("server",credentials.server_pin,context.certificate_expiry_unix_seconds()-10);
  publication(self,kForward);install(*node,self);node.reset();
  fs::copy(state/"metadata",backup,fs::copy_options::recursive,error);CHECK(!error);
  node=application(config,state);seed(*node,context,kForward,1,80);node.reset();
  node=application(config,state);CHECK(node->snapshot().committed_updates==80);node.reset();
  // Evidence, its 64-event checkpoint and tail remain valid. Only enrollment
  // metadata is restored to a version that lacks authenticated receipt objects.
  fs::remove_all(state/"metadata",error);CHECK(!error);
  fs::copy(backup,state/"metadata",fs::copy_options::recursive,error);CHECK(!error);
  node=std::make_unique<app::NativeMeshApplication>();CHECK(!node->open(config,state));
}
static void child_process_roundtrip(const fs::path& root,const fs::path& executable,
                                   const mesh::test::Credentials& credentials,tls::Context& server_context){
  namespace process=anaiero::process;
  const auto server_config=root/"server.cevmesh",server_state=root/"server-state";
  tls::Listener reservation;CHECK(reservation.open(server_context,{"127.0.0.1",0})==Code::ok);
  const auto port=reservation.local_endpoint().port;reservation.close();
  configuration(server_config,"mesh-server","server",credentials.server,port);
  process::NativeChildProcess server;
  CHECK(server.start({executable.string(),{"serve-stdio",server_config.string(),server_state.string()}})==process::Code::ok);
  std::string status;auto result=server.exchange("STATE",status,5000);
  check(result==process::Code::ok,server.diagnostics());CHECK(field(status,"projected_observations")==119);
  const auto config=root/"process-client.cevmesh",state=root/"process-client-state";
  configuration(config,"mesh-process-client","client",credentials.rotated_client,port,credentials.server_pin);
  auto receiver=application(config,state);
  auto grant=enrollment("server",credentials.server_pin,server_context.certificate_expiry_unix_seconds()-10);
  publication(grant,kForward);install(*receiver,grant);
  {
    app::NativeMeshClient client(*receiver);check(client.connect(),client.error());app::MeshTransferStats stats;
    check(client.synchronize("process",kForward,stats),client.error());CHECK(stats.records==112&&stats.duplicates==0);
    CHECK(receiver->snapshot().committed_updates==112);
  }
  receiver.reset();result=server.exchange("STATE",status,5000);check(result==process::Code::ok,server.diagnostics());
  CHECK(field(status,"projected_observations")==119);
  result=server.exchange("STOP",status,5000);check(result==process::Code::ok,server.diagnostics());CHECK(status=="{\"stopping\":true}");
  // EOF after STOP proves the child left its loop; no shell or local IPC socket
  // is needed for the private supervisor control channel.
  CHECK(server.exchange("STATE",status,5000)!=process::Code::ok);server.stop();
  auto reopened=application(server_config,server_state);CHECK(reopened->snapshot().committed_updates==119);
}

// Isolate infrastructure ownership/limits from production single-principal policy.
struct SerialGateway final:mesh::IMeshGateway {
  std::thread::id owner{};unsigned opens=0,closes=0,requests=0,admins=0;
  std::promise<void> entered,release;std::shared_future<void> gate=release.get_future().share();
  bool blocked=false;
  void check_owner(){if(owner==std::thread::id{})owner=std::this_thread::get_id();CHECK(owner==std::this_thread::get_id());}
  bool open_peer(const mesh::IAuthenticatedPeer& peer,std::uint64_t& id)override{check_owner();CHECK(peer.verified());id=++opens;return true;}
  std::string request(std::uint64_t,std::string_view value)override{check_owner();++requests;return std::string(value);}
  void close_peer(std::uint64_t)override{check_owner();++closes;}
  std::string administer(std::string_view value)override{
    check_owner();++admins;if(value=="BARRIER"){if(!blocked){blocked=true;entered.set_value();}gate.wait();}return std::string(value);
  }
};
static void bounded_host(tls::Context& server,tls::Context& client,const mesh::test::Credentials& credentials){
  SerialGateway gateway;mesh::NativeMeshHost host(gateway,server,{"127.0.0.1",0});CHECK(host.start()==Code::ok);
  std::array<tls::Connection,8> peers;
  for(auto& peer:peers)connect(peer,client,host.endpoint(),credentials.server_pin);
  std::array<std::thread,8> requests;
  for(std::size_t i=0;i<peers.size();++i)requests[i]=std::thread([&,i]{
    const auto message="peer "+std::to_string(i);CHECK(tls_exchange(peers[i],message)==message);});
  CHECK(administer(host,"STATE")=="STATE");for(auto& worker:requests)worker.join();
  tls::Connection ninth;CHECK(ninth.connect(client,host.endpoint(),credentials.server_pin,100)==Code::timeout);
  std::mutex mutex;std::condition_variable changed;unsigned busy=0;
  std::array<std::thread,24> administrators;
  const auto administration=[&]{
    const auto reply=administer(host,"BARRIER");
    if(reply=="CEVMESH/1 ERROR busy\n"){std::lock_guard lock(mutex);++busy;changed.notify_one();}
    else CHECK(reply=="BARRIER");
  };
  administrators[0]=std::thread(administration);gateway.entered.get_future().wait();
  for(std::size_t i=1;i<administrators.size();++i)administrators[i]=std::thread(administration);
  {std::unique_lock lock(mutex);CHECK(changed.wait_for(lock,3s,[&]{return busy==7;}));}
  gateway.release.set_value();for(auto& worker:administrators)worker.join();
  const auto stop=host.administration().handle("STOP");CHECK(stop.stop&&host.running());host.stop();CHECK(!host.running());
  CHECK(gateway.opens==8&&gateway.closes==8&&gateway.requests==8&&gateway.admins==19);
}
int main(int argc,char** argv){
  CHECK(argc==4);const auto root=fs::path(argv[1])/"mesh-integration";
  std::error_code error;fs::remove_all(root,error);CHECK(!error);fs::create_directories(root,error);CHECK(!error);
  mesh::test::Credentials credentials;CHECK(mesh::test::create_credentials(root/"credentials",credentials));
  tls::Context server,client;CHECK(server.open(credentials.server,tls::Role::server)==Code::ok);
  CHECK(client.open(credentials.client,tls::Role::client)==Code::ok);
  tls::Listener probe;const auto status=probe.open(server,{"127.0.0.1",0});
  if(status==Code::permission_denied){
    if(tls_required()){std::cerr<<"Required TCP/TLS unavailable\n";return 1;}
    std::cout<<"SKIP: environment denies native TCP sockets\n";return 77;
  }
  CHECK(status==Code::ok);probe.close();
  parser_regressions();production_roundtrip(root,credentials,server,client);
  missing_receipt_recovery(root,credentials,server);child_process_roundtrip(root,argv[2],credentials,server);
  bounded_host(server,client,credentials);
  std::cout<<"Native mesh integration: 119 observations, replay/ACK-loss recovery, rotation, artifacts and bounded ownership passed\n";
}
