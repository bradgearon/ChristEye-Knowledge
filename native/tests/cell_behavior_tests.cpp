#include "anaiero/infrastructure/cell_checkpoint.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/cell_run_plan.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
using namespace anaiero::cells;
namespace files=anaiero::cells::files;
namespace fs=std::filesystem;
#define CHECK(test) do {if(!(test)){std::cerr<<"FAILED " #test " line "<<__LINE__<<'\n'; std::exit(1);}}while(false)
static Atlas atlas;
struct Receipts final: IEventPort {
  std::uint64_t count=0;
  std::uint64_t link=0,nonviable=0,division=0;
  void published(const Event& event) noexcept override {
    ++count;
    link+=(event.kind==EventKind::linked?1u:0u);
    nonviable+=(event.kind==EventKind::nonviable?1u:0u);
    division+=(event.kind==EventKind::division?1u:0u);
  }
};
static void seed(CellBehaviorService& engine) {
  const auto add=[&](std::uint32_t id,Kind kind,int x,int y) {
    Cell c{};c.id=id;c.kind=kind;c.position={x,y,2};
    CHECK(engine.add(c)==Status::ok);
  };
  add(1,Kind::crest_progenitor,10,12);
  add(2,Kind::blood_carrier,3,5);
  add(3,Kind::central_neuron,20,17);
  add(4,Kind::central_neuron,23,17);
  CHECK(engine.encourage(3,4,700)==Status::ok);
}
static void identity_and_parser(const fs::path& p) {
  CHECK(files::load_checked_atlas(p,atlas)==Status::ok);
  CHECK(atlas.model_digest.size()==64);
  CHECK(atlas.by_kind[4].kind==Kind::central_neuron);
  CHECK(atlas.by_kind[4].contact_radius==9);
  CHECK(atlas.synthetic_only);
  std::ifstream file(p);std::string source((std::istreambuf_iterator<char>(file)),{});
  const auto prior=atlas.model_digest;
  auto changed=source;const auto where=changed.find("central_neuron");
  CHECK(where!=std::string::npos);changed[where]='C';
  CHECK(files::parse_atlas(changed,atlas)!=Status::ok);
  CHECK(atlas.model_digest==prior);
  changed=source;changed.replace(0,15,"CEVCELLATLAS/2");
  CHECK(files::parse_atlas(changed,atlas)!=Status::ok);
  CHECK(atlas.model_digest==prior);
}
static void replay_and_lineage(const fs::path& temp) {
  BoundedPositionPort movement;
  CellBehaviorService first(atlas,movement);
  Receipts receipts;first.set_observer(&receipts);
  seed(first);
  for(int i=0;i<10;++i)CHECK(first.step({75,0,64,true})==Status::ok);
  CHECK(first.world().tick==10);
  CHECK(first.world().cell_count==4);
  CHECK(first.world().link_count==1 && receipts.link==1);
  CHECK(first.world().links[0].from==3 && first.world().links[0].to==4);
  CHECK(first.world().cells[2].learning_proxy>0);
  CHECK(first.world().cells[1].position.x>3);
  CHECK(first.world().cells[0].maturity>=12);
  CHECK(first.transition(1,Kind::central_neuron)==Status::incompatible_lineage);
  CHECK(first.transition(1,Kind::peripheral_neuron)==Status::ok);
  CHECK(first.world().cells[0].kind==Kind::peripheral_neuron);
  CHECK(first.world().cells[0].stage==Stage::developing);
  const auto state=temp/"living.cevcells";
  CHECK(files::write_world(state,atlas,first.world())==Status::ok);
  World restored;
  CHECK(files::read_world(state,atlas,restored)==Status::ok);
  CHECK(restored.cell_count==4 && restored.link_count==1);
  CHECK(restored.cells[0].kind==Kind::peripheral_neuron);
  CellBehaviorService second(atlas,movement);
  CHECK(second.restore(restored)==Status::ok);
  for(int i=0;i<8;++i){
    CHECK(first.step({75,0,64,true})==Status::ok);
    CHECK(second.step({75,0,64,true})==Status::ok);
  }
  CHECK(files::serialize_world(first.world(),atlas)==files::serialize_world(second.world(),atlas));
  CHECK(first.world().cells[2].learning_proxy==second.world().cells[2].learning_proxy);
  auto malformed=files::serialize_world(restored,atlas);
  malformed[malformed.find("cell 1")+5]='9';
  World untouched=restored;
  CHECK(files::deserialize_world(malformed,atlas,untouched)!=Status::ok);
  CHECK(untouched.cells[0].kind==Kind::peripheral_neuron);
  CHECK(first.daughter(1,9)==Status::not_ready); // mature neuron is not a progenitor
}
static void developmental_and_depletion() {
  BoundedPositionPort movement;
  CellBehaviorService engine(atlas,movement);
  Cell founder{};founder.id=10;founder.kind=Kind::neural_progenitor;
  founder.position={20,20,2};
  CHECK(engine.add(founder)==Status::ok);
  CHECK(engine.daughter(10,11)==Status::not_ready);
  for(int i=0;i<9;++i)CHECK(engine.step({70,0,0,false})==Status::ok);
  CHECK(engine.daughter(10,11)==Status::ok);
  CHECK(engine.world().cells[1].id==11);
  CHECK(engine.world().cells[1].parent_id==10);
  CHECK(engine.world().cells[1].learning_proxy==0);
  CHECK(engine.transition(10,Kind::central_neuron)==Status::ok);
  CHECK(engine.transition(11,Kind::central_neuron)==Status::not_ready);
  for(int i=0;i<100;++i)CHECK(engine.step({0,200,0,false})==Status::ok);
  const auto stats=metrics(engine.world());
  CHECK(stats.viable==0 && stats.historical==2);
  CHECK(engine.world().cells[0].death_tick>0);
  CHECK(engine.world().cells[1].parent_id==10);
}
struct RejectSpatial final: ISpatialPort {
  bool translate(const Vec3&,const Vec3&,Vec3&) noexcept override{return false;}
};
static void movement_abort() {
  RejectSpatial reject;
  CellBehaviorService engine(atlas,reject);
  Cell cell{};cell.id=4;cell.kind=Kind::blood_carrier;cell.position={10,10,0};
  CHECK(engine.add(cell)==Status::ok);
  const World before=engine.world();
  CHECK(engine.step({70,0,0,false})==Status::movement_rejected);
  CHECK(engine.world().tick==before.tick && engine.world().cells[0].energy==before.cells[0].energy);
}
int main(int argc,char**argv) {
  CHECK(argc==2);
  const fs::path fixture=fs::path(argv[1])/"examples/cells/crest-blood-neural.cevatlas";
  identity_and_parser(fixture);
  files::RunPlan plan;
  CHECK(files::load_run_plan(fs::path(argv[1])/"examples/cells/reference-development.cevcellrun",atlas,plan)==Status::ok);
  CHECK(plan.total_ticks==18 && plan.count==3);
  CHECK(files::stage_for_tick(plan,0)->name=="developing");
  CHECK(files::stage_for_tick(plan,6)->name=="guided-contact");
  CHECK(files::stage_for_tick(plan,17)->name=="retention-probe");
  CHECK(files::stage_for_tick(plan,18)==nullptr);
  auto badplan=plan;
  CHECK(badplan.sha256.size()==64);
  files::SourceRecord reference;
  CHECK(files::load_source(fs::path(argv[1])/"examples/cells/synthetic-observations.cevsource",reference)==Status::ok);
  CHECK(files::validate_evidence(atlas,reference)==Status::ok);
  auto wrong=atlas;wrong.reference_digest=std::string(64,'0');
  CHECK(files::validate_evidence(wrong,reference)==Status::bad_profile);
  const auto temp=fs::temp_directory_path()/"anaiero-cell-profile-tests";
  std::error_code ec;fs::remove_all(temp,ec);fs::create_directories(temp);
  replay_and_lineage(temp);developmental_and_depletion();movement_abort();
  fs::remove_all(temp,ec);
  std::cout<<"Anaiero cell atlas synthetic developmental and replay tests passed\n";
}
