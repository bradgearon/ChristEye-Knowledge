// Real native model processes through mixed sessions, commit cuts, replay and
// exact restart. Reference equations qualify numerical plumbing, not biology.
#include "anaiero/application/native_session_application.hpp"
#include "anaiero/contracts/scientific_observable.hpp"
#include "anaiero/infrastructure/environment_model_file.hpp"
#include "anaiero/infrastructure/passive_model_file.hpp"
#include "anaiero/infrastructure/native_session_mesh_export.hpp"
#include "anaiero/infrastructure/mesh_wire.hpp"
#include "anaiero/services/environment_compartment_service.hpp"
#include "anaiero/services/passive_membrane_service.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace app=anaiero::application;
namespace sci=anaiero::science;
namespace session=anaiero::session;
namespace engine=anaiero::engine;
namespace runtime=anaiero::runtime;
namespace mesh=anaiero::mesh;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
static void write(const fs::path& path,std::string_view bytes){
  std::ofstream output(path,std::ios::binary);output<<bytes;CHECK(output.good());
}
static std::string read(const fs::path& path){
  std::string bytes;CHECK(db::read_bounded(path,65536,bytes));return bytes;
}
static fs::path solver_configuration(const fs::path& path,const fs::path& executable,
    const fs::path& model,std::string_view model_hash,std::string_view behavior,
    std::string_view provider,std::uint64_t step_ns,unsigned unit){
  std::ostringstream out;
  out<<"CEVSOLVER/1\nprovider "<<provider<<"\nmodel "<<std::quoted(model.string())<<' '<<model_hash
    <<"\nbehavior "<<christeye::living::plans::sha256_hex(behavior)<<"\nexecutable "<<std::quoted(executable.string())
    <<"\nstep_ns "<<step_ns<<"\nmax_steps 1000000\ntimeout_ms 2000\ninput_proxy 1000\nunit "<<unit
    <<"\ncheckpoint_schema 1\narguments 1\nmodel_argument\nend\n";
  write(path,out.str());return path;
}
static fs::path mixed_configuration(const fs::path& repo,const fs::path& path,
    const fs::path& membrane,const fs::path& environment){
  std::ostringstream out;
  out<<"CEVSESSION/2\nname environment-session\nperiod_ms 100\nstorage segmented 16\n"
    <<"engine 101 sw5 61 1 1 "<<std::quoted((repo/"examples/cells/crest-blood-neural.cevatlas").string())<<' '
    <<std::quoted((repo/"examples/runtime/two-neurons.cevcells").string())<<" 75 0 64 1\n"
    <<"engine 201 sim3 71 1 2 "<<std::quoted((repo/"examples/connected-gardens.cev").string())<<" \"-\" 0 0 0 0\n"
    <<"engine 301 scientific 81 9 2 "<<std::quoted(membrane.string())<<" \"-\" 0 0 0 0\n"
    <<"engine 302 scientific 82 7 3 "<<std::quoted(environment.string())<<" \"-\" 0 0 0 0\nend\n";
  write(path,out.str());return path;
}
static std::unique_ptr<app::NativeSessionApplication> open(const fs::path& config,const fs::path& root){
  auto result=std::make_unique<app::NativeSessionApplication>();const bool opened=result->open(config,root);
  if(!opened)std::cerr<<result->error()<<'\n';
  CHECK(opened);return result;
}
static void settle(app::NativeSessionApplication& value){
  const bool settled=value.settle(std::chrono::seconds(90));
  if(!settled)std::cerr<<value.driver().state_json();
  CHECK(settled&&!value.driver().failed()&&!value.driver().busy());
}
static session::Command step(std::uint64_t id,std::uint32_t count){
  session::Command command{};command.operation=session::Operation::step;
  command.operation_id=id;command.count=count;return command;
}
static runtime::MetricValue metric(const session::View& view,std::uint32_t source){
  for(std::size_t i=0;i<view.projected.count;++i)if(view.projected.values[i].source==source)return view.projected.values[i];
  CHECK(false);return {};
}
static void valid_view(const session::View& view,std::uint64_t cycles,
    std::string_view membrane_hash,std::string_view environment_hash){
  CHECK(view.cycles==cycles&&view.engine_count==4&&view.projected.count==12&&view.projected.sources==4);
  CHECK(view.projected.sequence_gaps==0&&view.pending_evidence==0);
  const auto& voltage=view.engines[2];const auto& concentration=view.engines[3];
  CHECK(voltage.working.tick==cycles/2&&concentration.working.tick==cycles/3);
  CHECK(voltage.capabilities.summaries[2]==engine::SummarySemantic::membrane_voltage_microvolt);
  CHECK(voltage.capabilities.input_schema==0xCE710001u&&voltage.capabilities.metric_schema==0xCE710002u);
  CHECK(concentration.capabilities.summaries[2]==engine::SummarySemantic::concentration_micromolar);
  CHECK(concentration.capabilities.input_schema==0xCE720001u&&concentration.capabilities.metric_schema==0xCE720002u);
  CHECK(voltage.capabilities.tick_ns==25000&&voltage.capabilities.clock_domain==81);
  CHECK(concentration.capabilities.tick_ns==100000000&&concentration.capabilities.clock_domain==82);
  CHECK(voltage.model_sha256==membrane_hash&&concentration.model_sha256==environment_hash);
  CHECK(voltage.classification=="NUMERICAL_MODEL_UNCALIBRATED"&&concentration.classification==voltage.classification);
  CHECK(voltage.capabilities.behavior_id==christeye::living::plans::sha256_hex(sci::kPassiveAnalyticBehavior));
  CHECK(concentration.capabilities.behavior_id==christeye::living::plans::sha256_hex(sci::kEnvironmentAnalyticBehavior));
  for(const auto index:{2u,3u}){
    const auto& entry=view.engines[index];const auto& caps=entry.capabilities;
    CHECK(!caps.isolated_fork&&!caps.evaluate_candidates&&!caps.apply_changes);
    CHECK(caps.summaries[0]==engine::SummarySemantic::unsupported&&caps.summaries[1]==engine::SummarySemantic::unsupported&&
      caps.summaries[3]==engine::SummarySemantic::unsupported);
    CHECK(entry.working.state_sha256==entry.published.state_sha256&&entry.working.revision==entry.coordinator_revision);
  }
  const auto membrane=metric(view,81),environment=metric(view,82);
  CHECK(membrane.metric==runtime::Metric::membrane_voltage&&membrane.source_epoch==9);
  CHECK(environment.metric==runtime::Metric::environmental_concentration&&environment.source_epoch==7);
  CHECK(membrane.sequence==cycles/2+1&&membrane.acquired_ns==(cycles/2)*25000);
  CHECK(environment.sequence==cycles/3+1&&environment.acquired_ns==(cycles/3)*100000000);
  CHECK(membrane.origin==runtime::Origin::simulated&&environment.origin==runtime::Origin::simulated);
  const double expected_voltage=-65000.0+79577472.0*20.0/1000000.0*
    (1-std::exp(-static_cast<double>(cycles/2)*0.025));
  const double expected_concentration=800.0-700.0*std::exp(-1.25*static_cast<double>(cycles/3)*0.1);
  CHECK(std::abs(static_cast<double>(membrane.value)-expected_voltage)<=1.0);
  CHECK(std::abs(static_cast<double>(environment.value)-expected_concentration)<=1.0);
  session::JsonSessionViewEncoder encoder;const auto json=encoder.encode(view);
  CHECK(json.size()<16384&&json.find("\"unit\":\"microvolt\"")!=std::string::npos&&
    json.find("\"unit\":\"micromolar\"")!=std::string::npos);
  CHECK(json.find("\"model_sha256\":\""+std::string(environment_hash)+"\"")!=std::string::npos&&
    json.find("concentration_micromolar")!=std::string::npos&&json.find("membrane_voltage_microvolt")!=std::string::npos);
  CHECK(json.find("NUMERICAL_MODEL_UNCALIBRATED")!=std::string::npos);
}
static void pump_to_environment_publication(app::NativeSessionApplication& value,std::uint64_t tick){
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(90);
  while(true){
    const auto& pending=value.service().record().publication;
    if(pending.active&&pending.engine_index==3&&pending.metric_index==0&&pending.snapshot.tick==tick)return;
    CHECK(value.driver().busy()&&!value.driver().failed()&&std::chrono::steady_clock::now()<deadline);
    const auto generation=value.wakeup().generation();
    if(!value.driver().pump())value.wakeup().wait(generation,deadline);
  }
}
static void same_final(const session::View& a,const session::View& b){
  CHECK(a.engine_count==b.engine_count&&a.cycles==b.cycles&&a.projected.committed_updates==b.projected.committed_updates);
  for(std::size_t i=0;i<a.engine_count;++i){
    CHECK(a.engines[i].working.state_sha256==b.engines[i].working.state_sha256);
    CHECK(a.engines[i].published.state_sha256==b.engines[i].published.state_sha256);
  }
  for(const auto source:{81u,82u}){
    const auto first=metric(a,source),second=metric(b,source);
    CHECK(first.metric==second.metric&&first.value==second.value&&first.evidence_tag==second.evidence_tag&&
      first.sequence==second.sequence&&first.acquired_ns==second.acquired_ns&&first.source_epoch==second.source_epoch);
  }
}
static void configuration_rejections(const fs::path& config,const fs::path& root){
  const auto original=read(config);std::string error;sci::ScientificConfiguration parsed;
  CHECK(sci::files::read_solver_configuration(config,parsed,error)&&parsed.expected.unit==sci::Unit::micromolar);
  // Unrecognized values must fail before narrowing to the uint8 unit enum.
  for(const auto unit:{0u,1u,8u,263u}){
    auto bytes=original;const auto at=bytes.find("\nunit 7\n");CHECK(at!=std::string::npos);
    bytes.replace(at,8,"\nunit "+std::to_string(unit)+"\n");
    const auto path=root/("invalid-unit-"+std::to_string(unit)+".cevsolver");write(path,bytes);
    CHECK(!sci::files::read_solver_configuration(path,parsed,error));
  }
  CHECK(sci::files::read_solver_configuration(config,parsed,error));
  // A supported but wrong unit cannot reinterpret the provider's READY reply.
  parsed.expected.unit=sci::Unit::microvolt;
  app::SupervisedScientificEngine wrong_unit(302,82,7,parsed);
  CHECK(wrong_unit.initialize()!=engine::Status::ok);
  parsed.expected.unit=sci::Unit::millimolar;
  app::SupervisedScientificEngine unsupported(302,82,7,parsed);
  CHECK(unsupported.initialize()!=engine::Status::ok&&unsupported.describe().capabilities.metric_schema==0);
}
static void mesh_preserves_observations(const fs::path& config,const fs::path& root,std::uint64_t cycles){
  std::uint64_t count=0,voltage_count=0,environment_count=0;std::string error;
  runtime::ColonyProjector remote_projection;
  const bool exported=mesh::export_native_session(config,root,[&](const runtime::Observation& original){
    mesh::wire::Request request;request.command=mesh::wire::Command::publish;request.observation=original;
    const auto bytes=mesh::wire::encode(request);mesh::wire::Request decoded;
    CHECK(!bytes.empty()&&mesh::wire::decode(bytes,decoded));
    CHECK(decoded.observation.metric==original.metric&&decoded.observation.value==original.value&&
      decoded.observation.source==original.source&&decoded.observation.source_epoch==original.source_epoch&&
      decoded.observation.clock_domain==original.clock_domain&&decoded.observation.evidence_tag==original.evidence_tag);
    CHECK(remote_projection.apply(decoded.observation)==runtime::Status::ok);
    if(original.source==81){CHECK(original.metric==runtime::Metric::membrane_voltage);++voltage_count;}
    if(original.source==82){CHECK(original.metric==runtime::Metric::environmental_concentration);++environment_count;}
    return true;
  },error,count);
  if(!exported)std::cerr<<error<<'\n';
  CHECK(exported&&count==5*(cycles+1)+6*(cycles/2+1)+(cycles/3+1));
  CHECK(voltage_count==cycles/2+1&&environment_count==cycles/3+1);
  const auto remote=remote_projection.snapshot(0,UINT64_MAX);
  CHECK(remote.count==12&&remote.sources==4&&remote.sequence_gaps==0&&remote.committed_updates==count);
}
int main(int argc,char** argv){
  CHECK(argc==5);
  const auto repo=fs::absolute(argv[1]),passive_provider=fs::absolute(argv[2]),environment_provider=fs::absolute(argv[3]),root=fs::absolute(argv[4]);
  fs::remove_all(root);fs::create_directories(root);
  static_assert(static_cast<unsigned>(runtime::Metric::membrane_voltage)==11);
  static_assert(static_cast<unsigned>(runtime::Metric::environmental_concentration)==12);
  static_assert(static_cast<unsigned>(engine::SummarySemantic::model_entities)==8);
  static_assert(static_cast<unsigned>(engine::SummarySemantic::concentration_micromolar)==9);
  const auto membrane_model=repo/"examples/scientific/passive-membrane.cevmodel";
  const auto environment_model=repo/"examples/scientific/well-mixed-environment.cevmodel";
  sci::PassiveMembraneModel membrane_parameters;sci::EnvironmentCompartmentModel environment_parameters;
  std::string membrane_hash,environment_hash;
  CHECK(sci::files::read_passive_model(membrane_model,membrane_parameters,membrane_hash));
  CHECK(sci::files::read_environment_model(environment_model,environment_parameters,environment_hash));
  const auto membrane=solver_configuration(root/"membrane.cevsolver",passive_provider,membrane_model,
    membrane_hash,sci::kPassiveAnalyticBehavior,"native-passive-analytic",25000,2);
  const auto environment=solver_configuration(root/"environment.cevsolver",environment_provider,environment_model,
    environment_hash,sci::kEnvironmentAnalyticBehavior,"native-environment-analytic",100000000,7);
  configuration_rejections(environment,root);
  const auto config=mixed_configuration(repo,root/"mixed.cevsession",membrane,environment);
  const auto state=root/"recovered";session::View first,final;
  {
    auto value=open(config,state);settle(*value);
    CHECK(value->driver().execute(step(1,12),nullptr));
    pump_to_environment_publication(*value,1);
    CHECK(metric(value->driver().view(),82).sequence==1);
    // Cut after model checkpoint/manifest intent but before evidence submission.
  }
  {
    auto value=open(config,state);
    CHECK(value->service().record().publication.active&&value->service().record().publication.snapshot.tick==1);
    pump_to_environment_publication(*value,2);
    const auto before=value->driver().view();CHECK(metric(before,82).sequence==2);
    CHECK(value->driver().pump()); // submit, deliberately do not poll completion
    const auto committed=value->committed_observations_after_shutdown();
    CHECK(committed==before.projected.committed_updates+1);
    CHECK(metric(value->driver().view(),82).sequence==2&&value->service().record().publication.metric_index==0);
    // Cut after journal commit but before projection/manifest acknowledgement.
  }
  {
    auto value=open(config,state);settle(*value);first=value->driver().view();
    valid_view(first,12,membrane_hash,environment_hash);
    const auto revision=first.revision;
    CHECK(value->driver().execute(step(1,12),nullptr));settle(*value);
    CHECK(value->driver().view().revision==revision&&value->driver().view().cycles==12);
    CHECK(value->driver().execute(step(2,12),nullptr));settle(*value);final=value->driver().view();
    valid_view(final,24,membrane_hash,environment_hash);
  }
  {
    auto value=open(config,root/"uninterrupted");settle(*value);
    CHECK(value->driver().execute(step(1,12),nullptr));settle(*value);same_final(first,value->driver().view());
    CHECK(value->driver().execute(step(2,12),nullptr));settle(*value);same_final(final,value->driver().view());
  }
  {
    auto value=open(config,state);settle(*value);valid_view(value->driver().view(),24,membrane_hash,environment_hash);
    same_final(final,value->driver().view());
  }
  {
    db::Registry metadata;CHECK(metadata.open(state/"session")==db::Code::ok);
    session::files::WindStoreRecordStore store(metadata,"session-environment-session");
    session::Record saved;bool found=false;CHECK(store.load(saved,found)&&found);
    CHECK(saved.contracts[2].summaries[2]==engine::SummarySemantic::membrane_voltage_microvolt&&
      saved.contracts[3].summaries[2]==engine::SummarySemantic::concentration_micromolar);
    saved.contracts[3].summaries[2]=static_cast<engine::SummarySemantic>(10);
    CHECK(session::files::encode_record(saved).empty());
  }
  mesh_preserves_observations(config,state,24);
  std::cout<<"native environment session, two commit cuts, mixed clocks, restart and mesh encoding passed\n";
}
