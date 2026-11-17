#include "anaiero/application/supervised_scientific_engine.hpp"
#include "anaiero/infrastructure/memory_checkpoint.hpp"
#include "anaiero/infrastructure/passive_model_file.hpp"
#include "anaiero/infrastructure/scientific_solver_configuration.hpp"
#include "anaiero/services/passive_membrane_service.hpp"
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace fs=std::filesystem;
namespace science=anaiero::science;
namespace engine=anaiero::engine;
namespace app=anaiero::application;
int main(int argc,char**argv){
  CHECK(argc==5);
  const auto python=fs::absolute(argv[1]),script=fs::absolute(argv[2]),model_path=fs::absolute(argv[3]),root=fs::absolute(argv[4]);
  fs::create_directories(root);
  science::PassiveMembraneModel model{};std::string model_hash,script_bytes;
  CHECK(science::files::read_passive_model(model_path,model,model_hash));
  CHECK(christeye::living::durable::read_bounded(script,1024*1024,script_bytes));
  const auto behavior=christeye::living::plans::sha256_hex(science::kNeuronPassiveBehavior);
  const auto script_hash=christeye::living::plans::sha256_hex(script_bytes);
  const auto config_path=root/"neuron.cevsolver";
  {std::ofstream file(config_path,std::ios::binary);
    file<<"CEVSOLVER/1\nprovider neuron-passive-9.0.1\nmodel "<<std::quoted(model_path.string())<<' '<<model_hash
      <<"\nbehavior "<<behavior<<"\nexecutable "<<std::quoted(python.string())
      <<"\nstep_ns 25000\nmax_steps 1000000\ntimeout_ms 10000\ninput_proxy 1000\nunit 2\ncheckpoint_schema 1\narguments 2\nfile_argument "
      <<std::quoted(script.string())<<' '<<script_hash<<"\nmodel_argument\nend\n";
  }
  science::ScientificConfiguration config;std::string error;
  CHECK(science::files::read_solver_configuration(config_path,config,error));
  app::SupervisedScientificEngine original(301,81,9,config);
  const auto initialized=original.initialize();
  if(initialized!=engine::Status::ok)std::cerr<<original.diagnostics()<<'\n';
  CHECK(initialized==engine::Status::ok);
  CHECK(original.describe().supports_checkpoint&&original.describe().supports_restore);
  CHECK(!original.describe().capabilities.isolated_fork&&!original.describe().capabilities.evaluate_candidates&&!original.describe().capabilities.apply_changes);
  const double gain=static_cast<double>(model.resistance_ohm)*static_cast<double>(model.max_current_picoamp)/1000000.0;
  double max_error=0;std::int64_t envelope=0;
  auto checked_step=[&](std::uint64_t tick){
    engine::Snapshot snapshot{};CHECK(original.advance({tick,{},tick+1},snapshot)==engine::Status::ok);
    science::Quantity quantity{};CHECK(original.quantity(quantity));
    CHECK(quantity.unit==science::Unit::microvolt&&quantity.source==81&&quantity.epoch==9);
    const double exact=static_cast<double>(model.rest_microvolt)+gain*(1-std::exp(-static_cast<double>((tick+1)*25000)/static_cast<double>(model.tau_ns)));
    const double difference=std::abs(static_cast<double>(quantity.value)-exact);
    max_error=std::max(max_error,difference);envelope=quantity.uncertainty;
    CHECK(difference<=static_cast<double>(quantity.uncertainty));
    CHECK(snapshot.energy==quantity.value&&snapshot.tick==tick+1);
    return snapshot;
  };
  for(std::uint64_t tick=0;tick<40;++tick)checked_step(tick);
  anaiero::infrastructure::MemoryCheckpoint checkpoint;
  CHECK(original.checkpoint(checkpoint)==engine::Status::ok);
  app::SupervisedScientificEngine restored(301,81,9,config);
  CHECK(restored.initialize()==engine::Status::ok&&restored.restore(checkpoint)==engine::Status::ok);
  engine::Snapshot first{},second{};
  CHECK(original.latest(first)==engine::Status::ok&&restored.latest(second)==engine::Status::ok);
  CHECK(first.state_sha256==second.state_sha256);
  for(std::uint64_t tick=40;tick<200;++tick){
    first=checked_step(tick);CHECK(restored.advance({tick,{},tick+1},second)==engine::Status::ok);
    CHECK(first.state_sha256==second.state_sha256&&first.energy==second.energy);
  }
  anaiero::infrastructure::MemoryCheckpoint a,b;
  CHECK(original.checkpoint(a)==engine::Status::ok&&restored.checkpoint(b)==engine::Status::ok&&a.bytes()==b.bytes());
  CHECK(max_error>0&&max_error<20); // Independent analytic reference, not just self-agreement.
  const auto report=root/"qualification.json";
  {std::ofstream file(report,std::ios::binary);
    file<<std::setprecision(12)<<"{\n  \"provider\": \"neuron-passive-9.0.1\",\n  \"classification\": \"NUMERICAL_MODEL_UNCALIBRATED\",\n"
      <<"  \"model_sha256\": \""<<model_hash<<"\",\n  \"behavior_sha256\": \""<<behavior<<"\",\n  \"script_sha256\": \""<<script_hash
      <<"\",\n  \"configuration_sha256\": \""<<config.configuration_sha256<<"\",\n  \"quantity\": \"membrane_voltage_microvolt\",\n  \"step_ns\": 25000,\n  \"samples\": 200,\n"
      <<"  \"max_analytic_error_microvolt\": "<<max_error<<",\n  \"numerical_envelope_microvolt\": "<<envelope
      <<",\n  \"restart_after_sample\": 40,\n  \"exact_continuation_samples\": 160,\n  \"opaque_checkpoint_equal_after_restart\": true\n}\n";
  }
  std::cout<<"NEURON 9.0.1 qualification PASS; 200 modeled voltage samples, max analytic error "<<max_error
    <<" microvolt, declared numerical envelope "<<envelope<<" microvolt; 160 checkpoint continuation samples exact\n";
}
