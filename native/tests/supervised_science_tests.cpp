#include "anaiero/application/supervised_scientific_engine.hpp"
#include "anaiero/infrastructure/scientific_solver_configuration.hpp"
#include "anaiero/infrastructure/memory_checkpoint.hpp"
#include "anaiero/infrastructure/passive_model_file.hpp"
#include "anaiero/services/passive_membrane_service.hpp"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace app=anaiero::application;
namespace science=anaiero::science;
namespace engine=anaiero::engine;
namespace process=anaiero::process;
namespace fs=std::filesystem;
static science::ScientificConfiguration configuration(const fs::path& executable,const fs::path& model,const std::string& fault=""){
  science::PassiveMembraneModel parsed;std::string sha;
  CHECK(science::files::read_passive_model(model,parsed,sha));
  science::ScientificConfiguration result;
  result.process.executable=executable.string();result.process.arguments={model.string()};
  if(!fault.empty())result.process.arguments.push_back(fault);
  result.expected={sha,christeye::living::plans::sha256_hex(science::kPassiveAnalyticBehavior),"native-passive-analytic",1,25000,1000000,science::Unit::microvolt,1};
  result.timeout_ms=2000;result.input_proxy=1000;return result;
}
static void advance(app::SupervisedScientificEngine& engine,std::uint64_t from,std::uint64_t count){
  engine::Snapshot state{};
  for(std::uint64_t i=0;i<count;++i)CHECK(engine.advance({from+i,{},from+i+1},state)==engine::Status::ok);
}
static void analytic_and_restart(const fs::path& executable,const fs::path& model){
  auto config=configuration(executable,model);
  app::SupervisedScientificEngine original(301,81,7,config);
  CHECK(original.initialize()==engine::Status::ok);
  CHECK(original.describe().capabilities.tick_ns==25000&&original.describe().supports_restore);
  CHECK(original.describe().capabilities.summaries[2]==engine::SummarySemantic::membrane_voltage_microvolt);
  advance(original,0,40);science::Quantity voltage{};CHECK(original.quantity(voltage));
  const auto expected=-65000.0+79577472.0*20.0/1000000.0*(1-std::exp(-1.0));
  CHECK(std::abs(static_cast<double>(voltage.value)-expected)<=static_cast<double>(voltage.uncertainty));
  CHECK(voltage.unit==science::Unit::microvolt&&voltage.source==81&&voltage.epoch==7);
  anaiero::infrastructure::MemoryCheckpoint checkpoint;
  CHECK(original.checkpoint(checkpoint)==engine::Status::ok);
  engine::Snapshot at_checkpoint{};CHECK(original.latest(at_checkpoint)==engine::Status::ok);
  CHECK(std::string(at_checkpoint.state_sha256.data())==christeye::living::plans::sha256_hex(checkpoint.bytes()));
  app::SupervisedScientificEngine resumed(301,81,7,config);
  CHECK(resumed.initialize()==engine::Status::ok&&resumed.restore(checkpoint)==engine::Status::ok);
  engine::Snapshot recovered{};CHECK(resumed.latest(recovered)==engine::Status::ok);
  CHECK(recovered.state_sha256==at_checkpoint.state_sha256&&recovered.tick==40);
  advance(original,40,60);advance(resumed,40,60);
  CHECK(original.latest(at_checkpoint)==engine::Status::ok&&resumed.latest(recovered)==engine::Status::ok);
  CHECK(at_checkpoint.state_sha256==recovered.state_sha256);
  anaiero::infrastructure::MemoryCheckpoint first,second;
  CHECK(original.checkpoint(first)==engine::Status::ok&&resumed.checkpoint(second)==engine::Status::ok&&first.bytes()==second.bytes());
  CHECK(resumed.advance({99,{},100},recovered)==engine::Status::tick_conflict);
  CHECK(resumed.advance({100,{1,0,0,false},101},recovered)==engine::Status::unsupported);
  auto bad=config;bad.expected.behavior_sha256[0]=bad.expected.behavior_sha256[0]=='a'?'b':'a';
  app::SupervisedScientificEngine mismatch(301,81,7,bad);CHECK(mismatch.initialize()!=engine::Status::ok);
  app::SupervisedScientificEngine wrong_epoch(301,81,8,config);
  CHECK(wrong_epoch.initialize()==engine::Status::ok&&wrong_epoch.restore(checkpoint)!=engine::Status::ok);
  std::string tampered(checkpoint.bytes());tampered[30]=tampered[30]=='a'?'b':'a';
  anaiero::infrastructure::MemoryCheckpoint damaged;CHECK(damaged.write(tampered));
  app::SupervisedScientificEngine wrong_bytes(301,81,7,config);
  CHECK(wrong_bytes.initialize()==engine::Status::ok&&wrong_bytes.restore(damaged)!=engine::Status::ok);
  app::SupervisedScientificEngine wrong_source(301,82,7,config);
  CHECK(wrong_source.initialize()==engine::Status::ok&&wrong_source.restore(checkpoint)!=engine::Status::ok);
  auto other_input=config;other_input.input_proxy=500;
  app::SupervisedScientificEngine wrong_input(301,81,7,other_input);
  CHECK(wrong_input.initialize()==engine::Status::ok&&wrong_input.restore(checkpoint)!=engine::Status::ok);
  auto limited=config;limited.expected.max_steps=2;
  app::SupervisedScientificEngine bounded(301,81,7,limited);
  CHECK(bounded.initialize()==engine::Status::ok);advance(bounded,0,2);
  anaiero::infrastructure::MemoryCheckpoint last;CHECK(bounded.checkpoint(last)==engine::Status::ok);
  app::SupervisedScientificEngine at_limit(301,81,7,limited);
  CHECK(at_limit.initialize()==engine::Status::ok&&at_limit.restore(last)==engine::Status::ok);
  CHECK(at_limit.latest(recovered)==engine::Status::ok&&recovered.tick==2);
  CHECK(at_limit.advance({2,{},3},recovered)!=engine::Status::ok&&at_limit.initialize()!=engine::Status::ok);
}
static void process_failures(const fs::path& executable,const fs::path& model){
  {
    auto config=configuration(executable,model);
    app::SupervisedScientificEngine idle(301,81,1,config);CHECK(idle.initialize()==engine::Status::ok);
    idle.cancel();advance(idle,0,1);idle.cancel();advance(idle,1,1);
    CHECK(idle.process_status()==process::Code::ok);
  }
  {
    process::NativeChildProcess blocked;CHECK(blocked.start({executable.string(),{model.string(),"no-read"}})==process::Code::ok);
    std::string response;const auto started=std::chrono::steady_clock::now();
    CHECK(blocked.exchange(std::string(process::kMaxFrameBytes-1,'x'),response,250)==process::Code::deadline);
    CHECK(std::chrono::steady_clock::now()-started<std::chrono::seconds(5));
  }
  for(const std::string fault:{"hang","partial","oversized","malformed","crash"}){
    auto config=configuration(executable,model,fault);config.timeout_ms=500;
    app::SupervisedScientificEngine value(301,81,1,config);CHECK(value.initialize()==engine::Status::ok);
    const auto started=std::chrono::steady_clock::now();engine::Snapshot snapshot{};
    CHECK(value.advance({0,{},1},snapshot)!=engine::Status::ok);
    CHECK(std::chrono::steady_clock::now()-started<std::chrono::seconds(5));
    CHECK(value.advance({0,{},1},snapshot)!=engine::Status::ok);
  }
  {
    auto config=configuration(executable,model,"handshake-hang");config.timeout_ms=300;
    app::SupervisedScientificEngine value(301,81,1,config);
    const auto start=std::chrono::steady_clock::now();CHECK(value.initialize()!=engine::Status::ok);
    CHECK(std::chrono::steady_clock::now()-start<std::chrono::seconds(5));
  }
  {
    auto config=configuration(executable,model,"hang");config.timeout_ms=10000;
    app::SupervisedScientificEngine value(301,81,1,config);CHECK(value.initialize()==engine::Status::ok);
    std::thread cancel([&]{std::this_thread::sleep_for(std::chrono::milliseconds(50));value.cancel();});
    const auto start=std::chrono::steady_clock::now();engine::Snapshot snapshot{};
    CHECK(value.advance({0,{},1},snapshot)!=engine::Status::ok);cancel.join();
    CHECK(value.process_status()==process::Code::cancelled);
    CHECK(std::chrono::steady_clock::now()-start<std::chrono::seconds(3));
  }
  {
    auto config=configuration(executable,model,"stderr");
    app::SupervisedScientificEngine value(301,81,1,config);CHECK(value.initialize()==engine::Status::ok);advance(value,0,1);
    CHECK(!value.diagnostics().empty()&&value.diagnostics().size()<=process::kMaxDiagnosticBytes);
  }
  {
    auto config=configuration(executable,model,"wrong-model");
    app::SupervisedScientificEngine value(301,81,1,config);CHECK(value.initialize()!=engine::Status::ok);
  }
  {
    process::NativeChildProcess value;CHECK(value.start({"/this/executable/does/not/exist",{}})==process::Code::start_failed);
    std::string reply;CHECK(value.exchange("HELLO",reply,100)!=process::Code::ok);
  }
}
static void config_files(const fs::path& executable,const fs::path& model,const fs::path& root){
  fs::create_directories(root/"path with spaces");const auto copied=root/"path with spaces"/"passive model.cevmodel";
  fs::copy_file(model,copied,fs::copy_options::overwrite_existing);
  auto c=configuration(executable,copied);const auto config_path=root/"native.cevsolver";
  {std::ofstream out(config_path,std::ios::binary);out<<"CEVSOLVER/1\nprovider "<<c.expected.provider<<"\nmodel "<<std::quoted(copied.string())<<' '<<c.expected.model_sha256
    <<"\nbehavior "<<c.expected.behavior_sha256<<"\nexecutable "<<std::quoted(executable.string())
    <<"\nstep_ns 25000\nmax_steps 1000000\ntimeout_ms 2000\ninput_proxy 1000\nunit 2\ncheckpoint_schema 1\narguments 1\nmodel_argument\nend\n";}
  science::ScientificConfiguration parsed;std::string error;
  CHECK(science::files::read_solver_configuration(config_path,parsed,error));
  CHECK(parsed.process.arguments.size()==1&&parsed.process.arguments[0]==copied.string()&&parsed.configuration_sha256.size()==64);
  app::SupervisedScientificEngine engine(301,81,1,parsed);CHECK(engine.initialize()==engine::Status::ok);advance(engine,0,2);
  {std::ofstream out(copied,std::ios::app|std::ios::binary);out<<' ';}
  CHECK(!science::files::read_solver_configuration(config_path,parsed,error));
}
int main(int argc,char**argv){
  CHECK(argc==4);const auto executable=fs::absolute(argv[1]);const auto model=fs::absolute(argv[2]);const auto root=fs::absolute(argv[3]);
  fs::remove_all(root);analytic_and_restart(executable,model);process_failures(executable,model);config_files(executable,model,root);
  std::cout<<"supervised native scientific model, deadlines, cancellation, provenance and checkpoint recovery PASS\n";
}
