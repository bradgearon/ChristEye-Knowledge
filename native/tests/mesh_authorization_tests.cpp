#include "anaiero/services/mesh_authorization_service.hpp"
#include "anaiero/services/session_authorization_service.hpp"
#include "anaiero/infrastructure/mesh_enrollment_store.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace mesh=anaiero::mesh;
namespace files=anaiero::mesh::files;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
struct Clock final:mesh::IUnixClock {
  std::uint64_t value=100;
  std::uint64_t unix_seconds()const noexcept override{return value;}
};
struct Peer final:mesh::IAuthenticatedPeer {
  bool trusted=true;std::string digest=std::string(64,'a');std::uint64_t expiry=900;
  bool verified()const noexcept override{return trusted;}
  std::string_view certificate_sha256()const noexcept override{return digest;}
  std::uint64_t certificate_expiry_unix_seconds()const noexcept override{return expiry;}
};
struct MemoryStore final:mesh::IEnrollmentStore {
  mesh::EnrollmentPolicy value{};bool fail_load=false,fail_save=false,fail_after_commit=false;
  bool load(mesh::EnrollmentPolicy& output,bool& found)noexcept override {
    if(fail_load)return false;
    found=value.revision!=0;output=value;return true;
  }
  mesh::EnrollmentStoreCode compare_exchange(std::uint64_t expected,const mesh::EnrollmentPolicy& next)noexcept override {
    if(fail_save)return mesh::EnrollmentStoreCode::failure;
    if(value.revision!=expected)return mesh::EnrollmentStoreCode::conflict;
    value=next;return fail_after_commit?mesh::EnrollmentStoreCode::failure:mesh::EnrollmentStoreCode::ok;
  }
};
struct Scratch {
  fs::path path;
  explicit Scratch(std::string_view label){
    const auto stamp=std::chrono::steady_clock::now().time_since_epoch().count();
    path=fs::temp_directory_path()/("anaiero-mesh-auth-"+std::string(label)+'-'+std::to_string(stamp));
    CHECK(fs::create_directories(path));
  }
  ~Scratch(){std::error_code ec;fs::remove_all(path,ec);}
};
static mesh::Enrollment enrollment(){
  mesh::Enrollment p;p.principal="node-a";p.certificate_sha256=std::string(64,'a');
  p.generation=1;p.expires_unix_s=1000;p.capabilities=mesh::kMeshCapabilityMask;
  p.publication_count=2;p.publications[0]={101,1,1,101};p.publications[1]={201,1,1,201};
  p.subscription_count=1;p.subscriptions[0]={301,1,3,301};
  p.artifact_count=1;p.artifact_sha256[0]=std::string(64,'f');return p;
}
static anaiero::runtime::Observation observation(mesh::Scope scope={101,1,1,101}){
  anaiero::runtime::Observation o;o.source=scope.source;o.source_epoch=scope.epoch;o.clock_domain=scope.clock;
  o.colony=scope.colony;o.sequence=1;o.metric=anaiero::runtime::Metric::activity;return o;
}
static void permissions(){
  MemoryStore store;Clock clock;Peer peer;mesh::MeshAuthorizationService auth(store,clock);
  CHECK(auth.start());CHECK(auth.upsert(enrollment(),0)==mesh::EnrollmentCode::ok);
  mesh::VerifiedMeshSession session;CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);
  CHECK(session.cryptographically_verified());CHECK(!session.expired());
  CHECK(auth.may_publish(session,observation()));CHECK(auth.may_publish(session,observation({201,1,1,201})));
  auto o=observation();++o.source;CHECK(!auth.may_publish(session,o));
  o=observation();++o.source_epoch;CHECK(!auth.may_publish(session,o));
  o=observation();++o.clock_domain;CHECK(!auth.may_publish(session,o));
  o=observation();++o.colony;CHECK(!auth.may_publish(session,o));
  o=observation();o.origin=anaiero::runtime::Origin::external_unverified_readonly;CHECK(!auth.may_publish(session,o));
  CHECK(auth.may_subscribe(session,{301,1,3,301}));CHECK(!auth.may_subscribe(session,{301,1,1,301}));
  CHECK(auth.may_read_artifact(session,std::string(64,'f')));CHECK(!auth.may_read_artifact(session,std::string(64,'e')));
  anaiero::security::SessionAuthorizationService old_policy;CHECK(!old_policy.may_publish(session,observation()));
  CHECK(!old_policy.may_change_digital_policy(session));
  mesh::VerifiedMeshSession bound;CHECK(auth.bind_publish(session,{101,1,1,101},bound));
  CHECK(old_policy.may_publish(bound,observation()));CHECK(!auth.may_publish(bound,observation({201,1,1,201})));
  CHECK(!auth.bind_publish(session,{999,1,1,999},bound));CHECK(!bound.cryptographically_verified());
  peer.trusted=false;mesh::VerifiedMeshSession denied;CHECK(auth.authenticate(peer,denied)==mesh::EnrollmentCode::denied);
  CHECK(!denied.cryptographically_verified());peer.trusted=true;peer.digest=std::string(64,'b');
  CHECK(auth.authenticate(peer,denied)==mesh::EnrollmentCode::denied);peer.digest=std::string(64,'a');
  peer.expiry=100;CHECK(auth.authenticate(peer,denied)==mesh::EnrollmentCode::expired);
  clock.value=900;CHECK(!auth.active(session));clock.value=100;CHECK(!auth.active(session));
}
static void rotation_revocation_reload(){
  MemoryStore store;Clock clock;Peer peer;mesh::MeshAuthorizationService auth(store,clock);CHECK(auth.start());
  auto p=enrollment();CHECK(auth.upsert(p,0)==mesh::EnrollmentCode::ok);
  CHECK(auth.upsert(p,1)==mesh::EnrollmentCode::ok);CHECK(auth.snapshot().revision==1);
  auto changed=p;changed.expires_unix_s=1100;CHECK(auth.upsert(changed,1)==mesh::EnrollmentCode::conflict);
  mesh::VerifiedMeshSession session;CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);
  CHECK(auth.rotate("node-a",std::string(64,'b'),1200,1)==mesh::EnrollmentCode::ok);
  CHECK(!auth.active(session));CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::denied);
  peer.digest=std::string(64,'b');CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);
  CHECK(auth.snapshot().peers[0].generation==2);
  CHECK(auth.revoke("node-a",2)==mesh::EnrollmentCode::ok);CHECK(!auth.active(session));
  CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::denied);
  CHECK(auth.revoke("node-a",3)==mesh::EnrollmentCode::ok);CHECK(auth.snapshot().revision==3);
  p=auth.snapshot().peers[0];p.revoked=false;++p.generation;CHECK(auth.upsert(p,3)==mesh::EnrollmentCode::ok);
  CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);
  CHECK(auth.reload());CHECK(!auth.active(session));CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);
  auto replacement=auth.snapshot();++replacement.revision;replacement.peers[0]={};replacement.count=0;
  CHECK(auth.replace(replacement,4)==mesh::EnrollmentCode::invalid);
  replacement=auth.snapshot();++replacement.revision;replacement.peers[0].revoked=true;
  CHECK(auth.replace(replacement,4)==mesh::EnrollmentCode::conflict);
  ++replacement.peers[0].generation;CHECK(auth.replace(replacement,4)==mesh::EnrollmentCode::ok);
  CHECK(!auth.active(session));
}
static void denied_capabilities(){
  for(const auto mask:{1u,2u,4u}){
    MemoryStore store;Clock clock;Peer peer;mesh::MeshAuthorizationService auth(store,clock);CHECK(auth.start());
    auto p=enrollment();p.capabilities=static_cast<std::uint8_t>(mask);
    if(!(mask&1)){p.publication_count=0;p.publications={};}
    if(!(mask&2)){p.subscription_count=0;p.subscriptions={};}
    if(!(mask&4)){p.artifact_count=0;p.artifact_sha256={};}
    CHECK(auth.upsert(p,0)==mesh::EnrollmentCode::ok);mesh::VerifiedMeshSession session;
    CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);
    CHECK(auth.may_publish(session,observation())==((mask&1)!=0));
    CHECK(auth.may_subscribe(session,{301,1,3,301})==((mask&2)!=0));
    CHECK(auth.may_read_artifact(session,std::string(64,'f'))==((mask&4)!=0));
  }
}
static void limits_and_expiry(){
  MemoryStore store;Clock clock;Peer peer;mesh::MeshAuthorizationService auth(store,clock);CHECK(auth.start());
  auto p=enrollment();p.expires_unix_s=150;CHECK(auth.upsert(p,0)==mesh::EnrollmentCode::ok);
  mesh::VerifiedMeshSession session;CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);
  clock.value=150;CHECK(!auth.may_read_artifact(session,std::string(64,'f')));
  CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::expired);
  auto duplicate=p;duplicate.principal="node-b";CHECK(auth.upsert(duplicate,1)==mesh::EnrollmentCode::invalid);
  duplicate.certificate_sha256=std::string(64,'b');CHECK(auth.upsert(duplicate,1)==mesh::EnrollmentCode::invalid);
  duplicate.publications[0].clock=3;CHECK(auth.upsert(duplicate,1)==mesh::EnrollmentCode::invalid);
  duplicate=p;duplicate.publications[1]=duplicate.publications[0];CHECK(!mesh::valid_enrollment(duplicate));
  duplicate=p;duplicate.capabilities=8;CHECK(!mesh::valid_enrollment(duplicate));
  duplicate=p;duplicate.artifact_count=17;CHECK(!mesh::valid_enrollment(duplicate));
  MemoryStore bounded_store;mesh::MeshAuthorizationService bounded(bounded_store,clock);CHECK(bounded.start());
  mesh::EnrollmentPolicy policy;policy.revision=1;policy.count=5;
  for(std::size_t i=0;i<policy.count;++i){auto& v=policy.peers[i];v.principal="peer-"+std::to_string(i);
    v.certificate_sha256=std::string(64,static_cast<char>('a'+i));v.generation=1;v.expires_unix_s=1000;
    v.capabilities=1;v.publication_count=i==4?1:8;
    for(std::size_t j=0;j<v.publication_count;++j)v.publications[j]={static_cast<std::uint32_t>(i*8+j+1),1,1,1};}
  CHECK(bounded.replace(policy,0)==mesh::EnrollmentCode::invalid);
  policy.peers[4]={};policy.count=4;CHECK(bounded.replace(policy,0)==mesh::EnrollmentCode::ok);
}
static void failure_closes_authority(){
  MemoryStore store;Clock clock;Peer peer;mesh::MeshAuthorizationService auth(store,clock);CHECK(auth.start());
  CHECK(auth.upsert(enrollment(),0)==mesh::EnrollmentCode::ok);mesh::VerifiedMeshSession session;
  CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);
  store.fail_save=true;CHECK(auth.revoke("node-a",1)==mesh::EnrollmentCode::persistence_failed);
  CHECK(!auth.ready()&&!auth.active(session));CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::denied);
  store.fail_save=false;CHECK(auth.reload());CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);
  store.fail_after_commit=true;CHECK(auth.revoke("node-a",1)==mesh::EnrollmentCode::persistence_failed);
  CHECK(!auth.ready()&&!auth.active(session));store.fail_after_commit=false;
  CHECK(auth.reload());CHECK(auth.snapshot().revision==2);CHECK(auth.snapshot().peers[0].revoked);
  store.fail_load=true;CHECK(!auth.reload());CHECK(!auth.ready());
}
static files::MeshConfiguration configuration(){
  files::MeshConfiguration c;c.name="mesh-example";c.principal="node-a";c.certificate_file="cert.pem";
  c.private_key_file="key.pem";c.ca_file="ca.pem";c.listen_port=17443;c.connect_port=17444;
  c.server_pin=std::string(64,'b');c.local_sources[0]={101,1,1,101};c.local_source_count=1;return c;
}
static void configurations(){
  auto c=configuration();const auto encoded=files::serialize_configuration(c);CHECK(!encoded.empty());
  files::MeshConfiguration parsed;std::string error;CHECK(files::parse_configuration(encoded,parsed,error));
  CHECK(files::serialize_configuration(parsed)==encoded);
  auto bad=encoded;bad[bad.find("node-a")]='x';CHECK(!files::parse_configuration(bad,parsed,error));
  CHECK(!files::parse_configuration(encoded+"extra\n",parsed,error));
  c.certificate_file="https://example.com/cert.pem";CHECK(files::serialize_configuration(c).empty());
  c=configuration();c.private_key_file="\\\\server\\share\\key.pem";CHECK(files::serialize_configuration(c).empty());
  c=configuration();c.connect_address="example.com";CHECK(files::serialize_configuration(c).empty());
  c=configuration();c.connect_address="127.00.0.1";CHECK(files::serialize_configuration(c).empty());
  c=configuration();c.connect_address="fe80::1%eth0";CHECK(files::serialize_configuration(c).empty());
  c=configuration();c.connect_address="::1";c.listen_address="2001:db8::1";CHECK(!files::serialize_configuration(c).empty());
  c.connect_address="::ffff:127.0.0.1";CHECK(!files::serialize_configuration(c).empty());
  c.connect_address="1:2:3:4:5:6:7:8:9";CHECK(files::serialize_configuration(c).empty());
  c.connect_address=":::1";CHECK(files::serialize_configuration(c).empty());
  c=configuration();c.deadline_ms=60001;CHECK(files::serialize_configuration(c).empty());
  mesh::EnrollmentPolicy policy;policy.revision=1;policy.count=1;policy.peers[0]=enrollment();
  const auto bytes=files::serialize_policy(policy);mesh::EnrollmentPolicy restored;
  CHECK(files::parse_policy(bytes,restored,error));CHECK(restored==policy);
  CHECK(!files::parse_policy(bytes+"extra\n",restored,error));
  bad=bytes;bad[bad.find("node-a")]='x';CHECK(!files::parse_policy(bad,restored,error));
  policy.peers[1]=policy.peers[0];policy.count=2;CHECK(files::serialize_policy(policy).empty());
  Scratch scratch("config");for(const auto* name:{"cert.pem","key.pem","ca.pem"}){std::ofstream out(scratch.path/name,std::ios::binary);out<<"fixture\n";}
  {std::ofstream out(scratch.path/"mesh.cevmesh",std::ios::binary);out<<encoded;}
  CHECK(files::load_configuration(scratch.path/"mesh.cevmesh",parsed,error));
  CHECK(parsed.certificate_path==fs::canonical(scratch.path/"cert.pem"));
  fs::remove(scratch.path/"key.pem");CHECK(!files::load_configuration(scratch.path/"mesh.cevmesh",parsed,error));
}
static void persistent_cas(){
  Scratch scratch("cas");Clock clock;Peer peer;
  {db::Registry database;CHECK(database.open(scratch.path,db::StorageOptions::sustained(8))==db::Code::ok);
    mesh::WindStoreEnrollmentStore store(database,"test");mesh::MeshAuthorizationService first(store,clock);CHECK(first.start());
    CHECK(first.upsert(enrollment(),0)==mesh::EnrollmentCode::ok);
    mesh::WindStoreEnrollmentStore second_store(database,"test");mesh::MeshAuthorizationService second(second_store,clock);CHECK(second.start());
    mesh::VerifiedMeshSession session;CHECK(second.authenticate(peer,session)==mesh::EnrollmentCode::ok);
    CHECK(first.revoke("node-a",1)==mesh::EnrollmentCode::ok);
    CHECK(second.rotate("node-a",std::string(64,'b'),1200,1)==mesh::EnrollmentCode::conflict);
    CHECK(!second.ready()&&!second.active(session));CHECK(second.reload());CHECK(second.snapshot().peers[0].revoked);}
  {db::Registry database;CHECK(database.open(scratch.path)==db::Code::ok);mesh::WindStoreEnrollmentStore store(database,"test");
    mesh::MeshAuthorizationService restored(store,clock);CHECK(restored.start());CHECK(restored.snapshot().revision==2);
    CHECK(restored.snapshot().peers[0].revoked);mesh::VerifiedMeshSession session;CHECK(restored.authenticate(peer,session)==mesh::EnrollmentCode::denied);}
}
static void durable_crash_cuts(){
  for(const bool segmented:{false,true})for(const auto cut:{db::FaultSite::after_journal_sync,db::FaultSite::after_head_temp_sync,db::FaultSite::after_head_commit}){
    Scratch scratch("crash");Clock clock;Peer peer;
    {db::Registry database;CHECK((segmented?database.open(scratch.path,db::StorageOptions::sustained(8)):database.open(scratch.path))==db::Code::ok);
      mesh::WindStoreEnrollmentStore store(database,"crash");mesh::MeshAuthorizationService auth(store,clock);CHECK(auth.start());
      CHECK(auth.upsert(enrollment(),0)==mesh::EnrollmentCode::ok);mesh::VerifiedMeshSession session;
      CHECK(auth.authenticate(peer,session)==mesh::EnrollmentCode::ok);database.inject_next_publish(cut);
      CHECK(auth.revoke("node-a",1)==mesh::EnrollmentCode::persistence_failed);CHECK(!auth.ready()&&!auth.active(session));}
    {db::Registry database;CHECK(database.open(scratch.path)==db::Code::ok);mesh::WindStoreEnrollmentStore store(database,"crash");
      mesh::MeshAuthorizationService auth(store,clock);CHECK(auth.start());const bool committed=cut==db::FaultSite::after_head_commit;
      CHECK(auth.snapshot().revision==(committed?2u:1u));CHECK(auth.snapshot().peers[0].revoked==committed);}
  }
}
int main(){permissions();rotation_revocation_reload();denied_capabilities();limits_and_expiry();failure_closes_authority();
  configurations();persistent_cas();durable_crash_cuts();std::cout<<"mesh authorization passed\n";}
