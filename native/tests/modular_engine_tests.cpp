#include "anaiero/sdk/engines.hpp"
#include "anaiero/sdk/experiments.hpp"
#include "christeye/living/learning_plan.hpp"
#include "christeye/living/colony_runner.hpp"
#include "anaiero/services/environment_compartment_service.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
using namespace anaiero::sdk;
namespace engines=anaiero::engines;
namespace pl=christeye::living::plans;
namespace sim=christeye::living::sim3;
namespace fs=std::filesystem;
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL " #x " line "<<__LINE__<<'\n';std::exit(1);}}while(false)
static fs::path root,temp;
static std::string read(const fs::path& path){
  std::ifstream in(path,std::ios::binary);CHECK(in.good());
  return {std::istreambuf_iterator<char>(in),{}};
}
static Envelope cell_config(){
  Envelope out;CHECK(engines::cell_configuration(read(root/"examples/cells/crest-blood-neural.cevatlas"),
    read(root/"examples/cells/synthetic-observations.cevsource"),read(root/"examples/runtime/two-neurons.cevcells"),out));return out;
}
static Envelope colony_config(){
  sim::Configuration model;CHECK(sim::Configuration::load(root/"examples/connected-gardens.cev",model)==sim::Result::ok);
  std::map<std::string,std::string> inputs{{model.material_path,read(root/"examples"/model.material_path)}};
  for(const auto& colony:model.colonies)inputs[colony.source_path]=read(root/"examples"/colony.source_path);
  std::vector<engines::RetainedAsset> assets;
  for(const auto& [path,bytes]:inputs)assets.push_back({path,bytes});
  Envelope out;CHECK(engines::colony_configuration(read(root/"examples/connected-gardens.cev"),assets,out));return out;
}
static Envelope plan_config(){
  pl::Plan plan;CHECK(pl::read_plan(root/"examples/vlm/bread-first.cevplan",plan).error==pl::Error::ok);
  std::vector<engines::RetainedAsset> assets;
  for(const auto& asset:plan.artifacts())assets.push_back({asset.relative_path,read(root/asset.relative_path)});
  Envelope out;CHECK(engines::plan_configuration(read(root/"examples/vlm/bread-first.cevplan"),
    read(root/"examples/vlm/plan-garden.avlm"),assets,out));return out;
}
static EngineModuleCatalog catalog;
static std::unique_ptr<IEngine> create(std::string_view type,const Envelope& config,
                                       const EngineState* restored=nullptr){
  EngineIdentity identity;CHECK(engines::identify(catalog,type,"engine","source",1,config,identity));
  std::unique_ptr<IEngine> out;const auto result=catalog.create(identity,config,restored,out);
  if(!result)std::cerr<<result.message<<'\n';
  CHECK(result);return out;
}
static PreparedTransition prepare(IEngine& engine,Envelope input,bool change=false){
  const auto before=engine.state();PreparedTransition out;
  StepRequest request{"op-"+std::to_string(before.revision),before.revision,std::move(input)};
  const auto result=change?engine.changes()->prepare_change(request,out):engine.prepare_step(request,out);
  if(!result)std::cerr<<result.message<<'\n';
  CHECK(result);
  CHECK(before==engine.state());CHECK(out.expected_state_sha256==state_digest(before));
  CHECK(out.target.revision==before.revision+1);CHECK(out.request_sha256==request_digest(engine.identity(),request));
  return out;
}
static void cell_staging_and_trials(){
  auto engine=create(engines::cell_behavior,cell_config());CHECK(engine->trials()&&engine->changes());
  const auto before=engine->state();
  auto step=prepare(*engine,engines::cell_input(75,0,64,true));CHECK(engine->install(step.target));
  CHECK(engine->state().tick==before.tick+1);CHECK(step.observations.size()==4);
  auto change=prepare(*engine,{"anaiero.cell-change/1","CECELLCHANGE/1 route 3 4 700\n"},true);
  CHECK(change.target.tick==engine->state().tick);CHECK(engine->install(change.target));
  const auto source=engine->state();
  TrialProgram program;program.id="candidate";program.inputs.assign(4,engines::cell_input(75,0,64,true));
  TrialResult result;CHECK(ExperimentService{}.evaluate(*engine,program,result));
  CHECK(engine->state()==source);CHECK(result.final_state.tick==source.tick+4);
  auto restore=create(engines::cell_behavior,source.configuration,&source);
  CHECK(prepare(*restore,engines::cell_input(75,0,64,true)).target==prepare(*engine,engines::cell_input(75,0,64,true)).target);
  PreparedTransition rejected;
  CHECK(!engine->prepare_step({"bad",engine->state().revision,{"anaiero.colony-input/1",{}}},rejected));
}
static void colony_staging_changes_and_recovery(){
  auto engine=create(engines::multi_colony,colony_config());
  fs::remove_all(temp/"legacy");sim::Runner reference;
  CHECK(reference.open(root/"examples/connected-gardens.cev",temp/"legacy")==sim::Result::ok);
  for(unsigned i=0;i<4;++i){
    auto next=prepare(*engine,engines::no_input(engines::multi_colony));
    CHECK(reference.step(false)==sim::Result::ok);
    CHECK(next.target.checkpoint.payload==sim::encode_snapshot(reference.state()));CHECK(engine->install(next.target));
  }
  const auto config_before=engine->state().configuration;
  const auto& edge=reference.configuration().links.front();
  const auto command="CECOLONYCHANGE/1 link "+edge.from+" "+edge.to+" "+sim::channel_text(edge.channel)+" 0\n";
  auto changed=prepare(*engine,{"anaiero.colony-change/1",command},true);
  CHECK(changed.target.configuration!=config_before);CHECK(changed.target.tick==4);CHECK(engine->install(changed.target));
  auto recovered=create(engines::multi_colony,changed.target.configuration,&changed.target);
  CHECK(prepare(*recovered,engines::no_input(engines::multi_colony)).target==prepare(*engine,engines::no_input(engines::multi_colony)).target);
  const auto& seed=reference.configuration().seeds.front();
  const auto teach="CECOLONYCHANGE/1 teach lesson-1 "+seed.colony+" "+std::to_string(seed.id)+" 100 6\n";
  auto scheduled=prepare(*engine,{"anaiero.colony-change/1",teach},true);CHECK(engine->install(scheduled.target));
  auto cancellation=prepare(*engine,{"anaiero.colony-change/1","CECOLONYCHANGE/1 cancel-teach lesson-1\n"},true);
  auto cancelled=create(engines::multi_colony,cancellation.target.configuration,&cancellation.target);
  CHECK(prepare(*cancelled,engines::no_input(engines::multi_colony)).receipts.empty());
  for(unsigned i=0;i<2;++i){auto next=prepare(*engine,engines::no_input(engines::multi_colony));
    CHECK(next.receipts.size()==(i==1?1U:0U));CHECK(engine->install(next.target));}
  PreparedTransition invalid;
  const auto before=engine->state();
  CHECK(engine->changes()->prepare_change({"cancel-executed",before.revision,
    {"anaiero.colony-change/1","CECOLONYCHANGE/1 cancel-teach lesson-1\n"}},invalid).code==Code::conflict);
  CHECK(engine->state()==before);
  auto restored=create(engines::multi_colony,before.configuration,&before);
  auto next=prepare(*restored,engines::no_input(engines::multi_colony));CHECK(next.receipts.empty());
  CHECK(!fs::exists(temp/"modern"/"culture.state")); // New model execution has no independent authoritative store.
}
static void cell_checkpoint_migration() {
  const auto config=cell_config();auto engine=create(engines::cell_behavior,config);
  auto* migration=engine->migrations();CHECK(migration);
  auto paused=engine->state();paused.phase=Phase::paused;paused.revision=1;
  CHECK(engine->restore(paused));
  MigrationRequest request{"cell-tagged-checkpoint",paused.revision,"CEVCELLSTATE/1","CEVCELLSTATE/2",
    std::string(migration->implementation_sha256()),{"anaiero.migration-parameters/1",{}}};
  const auto before=engine->state();PreparedTransition candidate;
  CHECK(migration->prepare_migration(request,candidate));
  CHECK(engine->state()==before);
  CHECK(candidate.identity==engine->identity()&&candidate.target.tick==before.tick);
  CHECK(candidate.target.revision==before.revision+1&&candidate.target.phase==Phase::paused);
  CHECK(candidate.target.checkpoint.schema=="CEVCELLSTATE/2");
  auto recovered=create(engines::cell_behavior,config,&candidate.target);
  auto corrupted=candidate.target;corrupted.checkpoint.payload.back()^=1;
  EngineIdentity identity=engine->identity();std::unique_ptr<IEngine> rejected;
  CHECK(!catalog.create(identity,config,&corrupted,rejected));
  auto resumed=candidate.target;resumed.phase=Phase::running;++resumed.revision;
  CHECK(recovered->restore(resumed));
  const auto next=prepare(*recovered,engines::cell_input(80,0,50,true));
  CHECK(next.target.checkpoint.schema=="CEVCELLSTATE/2");
  auto subsequent=create(engines::cell_behavior,config,&next.target);
  CHECK(subsequent->state()==next.target);
}
static void plan_matches_interpreter_and_every_tick_restart(){
  const auto config=plan_config();auto engine=create(engines::vlm_plan,config);
  const auto initial=engine->state();
  pl::Plan plan;christeye::living::process::ProcessMachine base;pl::Session reference;
  CHECK(pl::read_plan(root/"examples/vlm/bread-first.cevplan",plan).error==pl::Error::ok);
  CHECK(christeye::living::process::read_file(root/"examples/vlm/plan-garden.avlm",base).error==christeye::living::process::Error::ok);
  CHECK(reference.initialize(plan,base,root)==pl::Error::ok);
  std::ostringstream receipts;
  for(std::uint64_t tick=1;tick<=12;++tick){
    CHECK(reference.step()==pl::Error::ok);pl::write_progress_json(receipts,reference.progress().back());
    auto next=prepare(*engine,engines::no_input(engines::vlm_plan));
    CHECK(next.receipts.front().payload.ends_with("}\n"));
    CHECK(next.receipts.size()==(tick==12?2U:1U));
    // Every committed tick is recovered in a fresh engine using only retained bytes.
    engine=create(engines::vlm_plan,next.target.configuration,&next.target);
    std::string image,actual;CHECK(engines::plan_output(engine->state(),image,actual));
    CHECK(image==reference.machine().serialize(true));CHECK(actual==receipts.str());
  }
  CHECK(reference.finished()&&engine->state().phase==Phase::completed);
  PreparedTransition exhausted;CHECK(engine->prepare_step({"after-completion",12,engines::no_input(engines::vlm_plan)},exhausted).code==Code::completed);
  auto repeated=create(engines::vlm_plan,config,&initial);
  TrialProgram program;program.id="replay";program.inputs.assign(12,engines::no_input(engines::vlm_plan));
  TrialResult replayed;CHECK(ExperimentService{}.replay_equivalence(*repeated,program,engine->state(),replayed));
  CHECK(repeated->state()==initial);
  auto tampered=engine->state();tampered.checkpoint.payload.back()^=1;
  EngineIdentity identity=engine->identity();std::unique_ptr<IEngine> rejected;
  CHECK(!catalog.create(identity,tampered.configuration,&tampered,rejected));
  auto paused=initial;paused.phase=Phase::paused;paused.revision=1;
  auto paused_engine=create(engines::vlm_plan,config,&paused);
  CHECK(!paused_engine->prepare_step({"paused",1,engines::no_input(engines::vlm_plan)},exhausted));
  auto cancelled=initial;cancelled.phase=Phase::cancelled;cancelled.revision=1;
  auto cancelled_engine=create(engines::vlm_plan,config,&cancelled);
  CHECK(cancelled_engine->prepare_step({"cancelled",1,engines::no_input(engines::vlm_plan)},exhausted).code==Code::cancelled);
}
static void numerical_quantity_and_restore(){
  for(const auto type:{engines::passive_membrane,engines::environment}){
    const auto file=type==engines::environment?"examples/scientific/well-mixed-environment.cevmodel":"examples/scientific/passive-membrane.cevmodel";
    Envelope config;CHECK(engines::numerical_configuration(type,read(root/file),1000000,500,config));
    auto engine=create(type,config);CHECK(!engine->changes()&&!engine->trials());
    for(unsigned i=0;i<5;++i){auto next=prepare(*engine,engines::no_input(type));
      CHECK(next.observations.size()==1);CHECK(next.observations.front().unit==(type==engines::environment?"micromolar":"microvolt"));
      engine=create(type,next.target.configuration,&next.target);}
    auto a=prepare(*engine,engines::numerical_input(200));
    auto clone=create(type,engine->state().configuration,&engine->state());
    CHECK(a.target==prepare(*clone,engines::numerical_input(200)).target);
    auto identity=engine->identity();identity.tick_ns*=2;std::unique_ptr<IEngine> bad;
    CHECK(!catalog.create(identity,config,nullptr,bad));
  }
}
int main(int argc,char** argv){
  CHECK(argc==3);root=fs::canonical(argv[1]);temp=fs::absolute(argv[2]);fs::create_directories(temp);
  CHECK(engines::register_builtin_engines(catalog));CHECK(catalog.size()==5);CHECK(catalog.freeze());
  cell_staging_and_trials();cell_checkpoint_migration();colony_staging_changes_and_recovery();plan_matches_interpreter_and_every_tick_restart();numerical_quantity_and_restore();
  std::cout<<"Registered engine staging, VLM equivalence, complete SIM3 changes and numerical recovery passed\n";
}
