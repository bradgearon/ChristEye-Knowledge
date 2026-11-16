#include "christeye/living/wind_registry.hpp"
#include "christeye/wind/packet.hpp"
#include "christeye/wind/scheduler.hpp"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <cstdlib>
#include <string>

using namespace christeye::living;
namespace reg=christeye::living::registry;
namespace pl=christeye::living::plans;
namespace fs=std::filesystem;
#define CHECK(x) do { if(!(x)) {std::cerr<<"FAIL " #x << " line "<<__LINE__<<'\n';std::exit(1);} }while(false)

struct Bridge {
  christeye::wind::BoundedScheduler<32> pending{};
  std::uint64_t notified=0;
  unsigned fails=0;
  static void callback(void* context,const reg::Event& e) noexcept {
    auto& b=*static_cast<Bridge*>(context);
    christeye::wind::Header h{};
    h.kind=christeye::wind::Kind::evidence;
    h.delivery_class=christeye::wind::DeliveryClass::evidence;
    h.sequence=e.sequence;
    h.schema_id=0xCE600010u;
    h.payload_bytes=64;
    std::array<std::byte,64> payload{};
    for(unsigned i=0;i<64;++i)payload[i]=std::byte(static_cast<unsigned char>(e.hash[i]));
    std::array<std::byte,120> frame{};
    std::size_t count=0;
    christeye::wind::View view{};
    if(christeye::wind::encode(h,{},payload,frame,count)!=christeye::wind::Error::ok||
       christeye::wind::decode(std::span<const std::byte>(frame.data(),count),view)!=christeye::wind::Error::ok||
       view.header.sequence!=e.sequence||
       b.pending.post(christeye::wind::DeliveryClass::evidence,{e.sequence,0})!=christeye::wind::PostResult::accepted)
      ++b.fails;
    else ++b.notified;
  }
};
static fs::path testdir(){
  const auto dir=fs::temp_directory_path()/"christeye-wind-registry-test";
  std::error_code ec; fs::remove_all(dir,ec);fs::create_directories(dir);
  return dir;
}
static void write_text(const fs::path& path,std::string_view data){
  std::ofstream file(path,std::ios::binary|std::ios::trunc);
  CHECK(static_cast<bool>(file));file.write(data.data(),static_cast<std::streamsize>(data.size()));
  file.close();CHECK(static_cast<bool>(file));
}
static void tests(const fs::path& source_root){
  auto dir=testdir();reg::Registry store;
  CHECK(store.open(dir)==reg::Code::ok);
  CHECK(store.events().empty());
  const auto bytes=store.put("CERNA/1\norigin SIMULATED\n");
  CHECK(bytes.status==reg::Code::ok);
  CHECK(bytes.hash==pl::sha256_hex("CERNA/1\norigin SIMULATED\n"));
  CHECK(store.put("CERNA/1\norigin SIMULATED\n").hash==bytes.hash);
  std::string loaded;CHECK(store.get(bytes.hash,loaded)&&loaded=="CERNA/1\norigin SIMULATED\n");
  Bridge bridge;
  CHECK(store.subscribe(Bridge::callback,&bridge)==reg::Code::ok);
  const auto one=store.publish("RNA_OBSERVATION","binding-1",bytes.hash,"SIMULATED",7,"sim-clock");
  CHECK(one.status==reg::Code::ok && bridge.notified==1);
  const auto retry=store.publish("RNA_OBSERVATION","binding-1",bytes.hash,"SIMULATED",7,"sim-clock");
  CHECK(retry.hash==one.hash && bridge.notified==1 && store.events().size()==1);
  CHECK(store.publish("RNA_OBSERVATION","binding-1",bytes.hash,"SIMULATED",8,"sim-clock").status==reg::Code::ok);
  CHECK(store.publish("RNA_OBSERVATION","binding-1","f"+bytes.hash,"SIMULATED",9,"sim-clock").status==reg::Code::bad_argument);
  CHECK(store.publish("RNA_OBSERVATION","binding-1",std::string(64,'1'),"SIMULATED",9,"sim-clock").status==reg::Code::missing_object);
  CHECK(store.publish("RNA_OBSERVATION","binding-1",bytes.hash,"LIVE_BIOLOGICAL",9,"sim-clock").status==reg::Code::bad_argument);
  CHECK(store.verify());
  CHECK(bridge.pending.next(1).item.token==1);
  CHECK(bridge.pending.next(1).item.token==2);
  CHECK(bridge.pending.pending()==0);
  reg::Registry reopened;
  CHECK(reopened.open(dir)==reg::Code::ok);
  CHECK(reopened.events().size()==2 && reopened.verify());
  Bridge resumed;
  CHECK(reopened.subscribe(Bridge::callback,&resumed,1)==reg::Code::ok);
  CHECK(resumed.notified==1);
  Bridge filtered;
  CHECK(reopened.subscribe(Bridge::callback,&filtered,0,"binding-1","RNA_OBSERVATION")==reg::Code::ok);
  CHECK(filtered.notified==2);
  CHECK(reopened.inbox("binding-1",1,"RNA_OBSERVATION").size()==1);
  CHECK(reopened.inbox("other-binding").empty());
  // A peer can import a verified immutable object without claiming a secure network.
  reg::Registry peer;
  CHECK(peer.open(dir/"peer")==reg::Code::ok);
  CHECK(peer.put_file(dir/"objects"/bytes.hash,bytes.hash).status==reg::Code::ok);
  CHECK(peer.publish("RNA_OBSERVATION","binding-1",bytes.hash,"IMPORTED_UNVERIFIED",7,"peer-clock").status==reg::Code::ok);
  CHECK(peer.verify());
  CHECK(peer.put_file(dir/"objects"/bytes.hash,std::string(64,'0')).status==reg::Code::bad_digest);

  // Register actual CEVPLAN fixtures (contains synthetic RNA, digital Crystal, etc.).
  pl::Plan plan;
  const auto plan_file=source_root/"examples/vlm/bread-first.cevplan";
  CHECK(pl::read_plan(plan_file,plan).error==pl::Error::ok);
  reg::StageMapping mapping;
  CHECK(reg::resolve_stage(plan,plan.stages().front().id,mapping));
  CHECK(mapping.plan==plan.hash());
  CHECK(reg::valid_digest(mapping.crystal) && reg::valid_digest(mapping.rna));
  CHECK(!reg::resolve_stage(plan,"missing-stage",mapping));
  const auto imported=reopened.register_plan(plan,source_root);
  CHECK(imported.status==reg::Code::ok);
  std::string plan_bytes;CHECK(reopened.get(imported.hash,plan_bytes));
  CHECK(plan_bytes==plan.serialize(true));
  for(const auto& artifact:plan.artifacts()){
    std::string content;CHECK(reopened.get(artifact.sha256,content));
    CHECK(pl::sha256_hex(content)==artifact.sha256);
  }
  auto learning=reopened.put("CELEARN/1\nrevision successor-demo\n");
  CHECK(learning.status==reg::Code::ok);
  const auto successor=reg::successor_manifest(imported.hash,learning.hash,bytes.hash,"successor-one");
  CHECK(successor.starts_with("CEVSUCCESSOR/1\n") && successor.find(imported.hash)!=std::string::npos);
  const auto su=reopened.put(successor);
  CHECK(su.status==reg::Code::ok);
  const auto posted=reopened.publish("SUCCESSOR_PROPOSED","successor-one",su.hash,"SIMULATED",11,"sim-clock");
  CHECK(posted.status==reg::Code::ok);
  CHECK(reopened.verify());
  CHECK(filtered.notified==2); // plan and successor on other entities are excluded.
  CHECK(reopened.inbox("successor-one",0,"SUCCESSOR_PROPOSED").size()==1);
  reg::Registry restart; CHECK(restart.open(dir)==reg::Code::ok && restart.verify());
  CHECK(restart.events().size()==4);
  CHECK(restart.get(imported.hash,plan_bytes));
  CHECK(plan_bytes==plan.serialize(true)); // predecessor is never overwritten
  // Corrupt journal and demonstrate fail-closed startup.
  {
    std::ofstream append(dir/"events.log",std::ios::app|std::ios::binary);
    append<<"incomplete";
  }
  reg::Registry bad;CHECK(bad.open(dir)==reg::Code::corrupt_journal);
  CHECK(bad.events().empty());
  // Repair the test fixture then corrupt a referenced CAS object.
  std::string journal;
  CHECK(pl::read_bounded(dir/"events.log",reg::kMaxRegistryJournalBytes,journal));
  journal.resize(journal.size()-std::string("incomplete").size());
  write_text(dir/"events.log",journal);
  write_text(dir/"objects"/bytes.hash,"attacker-modified");
  CHECK(bad.open(dir)==reg::Code::corrupt_journal);
  std::error_code ec;fs::remove_all(dir,ec);
}
int main(int argc,char**argv){
  CHECK(argc==2);
  tests(argv[1]);
  std::cout<<"WindRegistry CAS, journal replay, subscriptions and successor tests passed\n";
}
