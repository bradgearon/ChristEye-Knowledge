#include "christeye/living/learning_plan.hpp"
#include "christeye/wind/packet.hpp"
#include "christeye/wind/scheduler.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using namespace christeye::living;
namespace pl=christeye::living::plans;
namespace p=christeye::living::process;
namespace v=christeye::living::vlm;
namespace w=christeye::wind;
#define REQUIRE(x) do { if(!(x)){std::cerr<<"FAIL " #x " line "<<__LINE__<<'\n';std::exit(1);} } while(false)
static std::filesystem::path root;
static p::ProcessMachine make_base(){
 p::ProcessMachine base;
 REQUIRE(p::read_file(root/"examples/vlm/plan-garden.avlm",base).error==p::Error::ok);
 return base;
}
static pl::Plan make_plan(){
 pl::Plan plan;
 auto r=pl::read_plan(root/"examples/vlm/bread-first.cevplan",plan);
 REQUIRE(r.error==pl::Error::ok);
 REQUIRE(plan.stages().size()==3);
 REQUIRE(plan.bindings().size()==3);
 REQUIRE(plan.artifacts().size()==7);
 REQUIRE(plan.verify_assets(root)==pl::Error::ok);
 return plan;
}
static void sha_vectors(){
 REQUIRE(pl::sha256_hex("")=="e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
 REQUIRE(pl::sha256_hex("abc")=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}
struct Recorder {
 w::BoundedScheduler<32> scheduler{};
 std::uint32_t callbacks=0,failures=0;
 std::string last_receipt;
 static void emit(void* context,const pl::Progress& event) noexcept {
   auto& recorder=*static_cast<Recorder*>(context);
   w::Header h{};
   h.kind=w::Kind::evidence;
   h.delivery_class=w::DeliveryClass::evidence;
   h.stream_id=91;
   h.schema_id=0xCE600009u;
   h.sequence=event.tick;
   h.payload_bytes=32;
   std::array<std::byte,32> payload{};
   auto nibble=[](char c) noexcept -> unsigned {
     return c>='0' && c<='9' ? unsigned(c-'0') : unsigned(c-'a'+10);
   };
   for(unsigned i=0;i<32;++i){
      payload[i]=std::byte((nibble(event.receipt_hash[2*i])<<4) |
                            nibble(event.receipt_hash[2*i+1]));
   }
   std::array<std::byte,88> encoded{};
   std::size_t written=0;
   w::View view{};
   if(w::encode(h,{},payload,encoded,written)!=w::Error::ok ||
      w::decode(std::span<const std::byte>(encoded.data(),written),view)!=w::Error::ok ||
      view.header.sequence!=event.tick ||
      recorder.scheduler.post(w::DeliveryClass::evidence,{event.task_id,0})!=w::PostResult::accepted){
      ++recorder.failures;return;
   }
   recorder.last_receipt=event.receipt_hash;
   ++recorder.callbacks;
 }
};
static void run_and_replay(){
 const auto plan=make_plan();auto base=make_base();
 REQUIRE(pl::machine_sha256(base)==plan.base_sha());
 pl::Session session;
 REQUIRE(session.initialize(plan,base,root)==pl::Error::ok);
 Recorder recorder;
 session.subscribe(Recorder::emit,&recorder);
 while(!session.finished()) REQUIRE(session.step()==pl::Error::ok);
 REQUIRE(recorder.failures==0 && recorder.callbacks==12);
 REQUIRE(recorder.scheduler.pending()==12);
 REQUIRE(session.machine().tick()==12);
 REQUIRE(session.machine().base().core().cell(1)->learning_proxy==25);
 REQUIRE(session.machine().base().soil().total(v::FieldKind::support)==300);
 REQUIRE(session.machine().base().soil().total(v::FieldKind::rna)==200);
 REQUIRE(session.progress().at(2).completed);
 REQUIRE(session.progress().at(6).completed);
 REQUIRE(session.progress().back().completed);
 for(const auto& e:session.progress()){
   REQUIRE(e.crystal_hash==plan.artifact("crystal-17")->sha256);
   REQUIRE(e.rna_hash==plan.artifact("rna-17")->sha256);
   REQUIRE(e.plan_hash==plan.hash());
   REQUIRE(e.receipt_hash.size()==64);
 }
 const auto state=session.machine().serialize(true);
 p::ProcessMachine restored;
 REQUIRE(p::ProcessMachine::deserialize(state,restored,true).error==p::Error::ok);
 REQUIRE(restored.state_hash()==session.machine().state_hash());
 pl::Session repeat;
 REQUIRE(repeat.initialize(plan,base,root)==pl::Error::ok);
 while(!repeat.finished())REQUIRE(repeat.step()==pl::Error::ok);
 REQUIRE(repeat.machine().serialize(true)==state);
 REQUIRE(repeat.progress().back().receipt_hash==session.progress().back().receipt_hash);
}
static void invalid_files_and_mappings(){
 auto plan=make_plan();auto base=make_base();
 pl::Session session;
 REQUIRE(session.initialize(plan,base,root)==pl::Error::ok);
 REQUIRE(base.name("changed-name"));
 REQUIRE(session.initialize(plan,base,root)==pl::Error::base_mismatch);
 auto text=plan.serialize(true);
 text.replace(text.find("binding b-18"),7,"binding x-18");
 pl::Plan sentinel=plan;
 REQUIRE(pl::Plan::parse(text,sentinel).error!=pl::Error::ok);
 REQUIRE(sentinel.hash()==plan.hash());
 text=plan.serialize(true);
 const auto p=text.find("digital_learning");
 REQUIRE(p!=std::string::npos);
 text.replace(p,16,"rna_observation ");
 REQUIRE(pl::Plan::parse(text,sentinel).error==pl::Error::plan_hash_mismatch);
 text=plan.serialize(true);
 REQUIRE(pl::Plan::parse(text,sentinel).error==pl::Error::ok);
 REQUIRE(pl::Plan::parse(text.substr(0,text.find("seal ")),sentinel).error!=pl::Error::ok);
 auto wrong=text;
 const auto st=wrong.find("stage support-wave");
 REQUIRE(st!=std::string::npos);
 wrong.replace(st,5,"empty");
 REQUIRE(pl::Plan::parse(wrong,sentinel).error!=pl::Error::ok);
 REQUIRE(!pl::valid_file_path("../secret"));
 REQUIRE(!pl::valid_file_path("/etc/passwd"));
 REQUIRE(!pl::valid_file_path("https://somewhere/file"));
 REQUIRE(pl::valid_file_path("docs/assets/vlm/crest-glow.svg"));
}
static void asset_integrity_and_semantic_gates(){
  const auto plan=make_plan();
  const auto original=root/"examples/vlm/data/lesson-demo.txt";
  const auto temp=std::filesystem::temp_directory_path()/"anaiero-vlm-plan-integrity";
  std::error_code ec;
  std::filesystem::remove_all(temp,ec);
  std::filesystem::create_directories(temp/"examples/vlm/data");
  std::filesystem::create_directories(temp/"docs/assets/vlm");
  for(const auto& art:plan.artifacts()) {
    const auto to=temp/art.relative_path;
    std::filesystem::create_directories(to.parent_path());
    std::filesystem::copy_file(root/art.relative_path,to);
  }
  REQUIRE(plan.verify_assets(temp)==pl::Error::ok);
  {
    std::ofstream tamper(temp/"examples/vlm/data/lesson-demo.txt",std::ios::app);
    tamper << "changed\n";
  }
  REQUIRE(plan.verify_assets(temp)==pl::Error::asset_hash_mismatch);
  std::filesystem::copy_file(original,temp/"examples/vlm/data/lesson-demo.txt",
                             std::filesystem::copy_options::overwrite_existing);
  REQUIRE(plan.verify_assets(temp)==pl::Error::ok);
  // A digest-valid file with the wrong declared record role is also rejected.
  // The modified artifact has a correct digest, but the contents are mislabeled.
  REQUIRE(plan.verify_assets(temp)==pl::Error::ok);
  std::filesystem::remove_all(temp,ec);
}
static void plan_mapping_conflicts_fail_closed(){
  auto plan=make_plan();
  auto raw=plan.serialize(false);
  auto at=raw.find("stage rna-wave 703 rna-stream b-19");
  REQUIRE(at!=std::string::npos);
  raw.replace(at,std::string("stage rna-wave 703 rna-stream b-19").size(),
              "stage rna-wave 703 rna-stream b-18");
  const auto resealed=raw+"seal "+pl::sha256_hex(raw)+"\n";
  pl::Plan parsed;
  REQUIRE(pl::Plan::parse(resealed,parsed).error==pl::Error::duplicate_id);
  auto base=make_base();
  REQUIRE(base.name("new-name"));
  pl::Session session;
  REQUIRE(session.initialize(plan,base,root)==pl::Error::base_mismatch);
  // A host cannot treat the plan as bound to a different copy of an image
  // simply because its synthetic cell counts look identical.
}
int main(int argc,char** argv){
 REQUIRE(argc==2);
 root=std::filesystem::canonical(argv[1]);
 sha_vectors();run_and_replay();invalid_files_and_mappings();
 asset_integrity_and_semantic_gates();plan_mapping_conflicts_fail_closed();
 std::cout<<"CEVPLAN/1 hashes, typed mappings, VLM2 replay and WindPacket tests passed\n";
}
