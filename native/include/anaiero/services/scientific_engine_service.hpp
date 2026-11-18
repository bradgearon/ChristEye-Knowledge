#pragma once
#include "anaiero/contracts/simulation_engine.hpp"
#include "anaiero/contracts/supervised_solver.hpp"
#include "anaiero/contracts/scientific_observable.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include "christeye/living/plan_hash.hpp"
#include <algorithm>
#include <sstream>
namespace anaiero::science {
// Native domain lifecycle around a source-qualified model process. The injected
// solver owns model details; this service owns sequence/identity and capabilities.
class ScientificEngineService final:public engine::ISimulationEngine {
  ISupervisedSolver& solver_;SolverIdentity identity_;
  std::uint32_t id_,source_;
  std::int32_t configured_input_;
  bool initialized_=false;
  contracts::IServiceExtension* extension_=nullptr;
  bool checkpoint_bytes(std::string& output)const {
    std::string solver;if(!solver_.checkpoint(solver))return false;
    std::ostringstream header;header<<"CEVSCIENCEENGINE/1\n"<<id_<<' '<<source_<<' '<<identity_.epoch<<' '
      <<configured_input_<<' '<<identity_.model_sha256<<' '<<identity_.behavior_sha256<<' '
      <<identity_.provider<<' '<<identity_.step_ns<<' '<<static_cast<unsigned>(identity_.unit)<<' '
      <<identity_.checkpoint_schema<<"\nbytes "<<solver.size()<<'\n';
    const auto body=header.str()+solver;
    output=body+"seal "+christeye::living::plans::sha256_hex(body)+"\n";return true;
  }
  bool solver_checkpoint(std::string_view bytes,std::string_view& solver)const {
    if(bytes.size()<70||bytes.size()>16384)return false;
    const auto seal=bytes.size()-70;
    if(bytes.substr(seal,5)!="seal "||bytes.back()!='\n'||
       christeye::living::plans::sha256_hex(bytes.substr(0,seal))!=bytes.substr(seal+5,64))return false;
    std::istringstream input{std::string(bytes.substr(0,seal))};
    std::string header,model,behavior,provider,tag,extra;
    std::uint32_t id=0,source=0;std::uint64_t epoch=0,step=0;std::int32_t configured_input=0;
    unsigned unit=0,schema=0;std::size_t count=0;
    if(!std::getline(input,header)||header!="CEVSCIENCEENGINE/1"||
       !(input>>id>>source>>epoch>>configured_input>>model>>behavior>>provider>>step>>unit>>schema>>tag>>count)||
       id!=id_||source!=source_||epoch!=identity_.epoch||configured_input!=configured_input_||
       model!=identity_.model_sha256||behavior!=identity_.behavior_sha256||provider!=identity_.provider||
       step!=identity_.step_ns||unit!=static_cast<unsigned>(identity_.unit)||schema!=identity_.checkpoint_schema||
       tag!="bytes"||!count||count>8192||!std::getline(input,extra)||!extra.empty())return false;
    const auto position=input.tellg();if(position<0)return false;
    const auto offset=static_cast<std::size_t>(position);
    if(offset>seal||count!=seal-offset)return false;
    solver=bytes.substr(offset,count);return true;
  }
  std::string snapshot_digest(const SolverSample& sample,std::string_view checkpoint)const{
    // Checkpoint-capable providers bind the complete opaque numerical state.
    // Non-restorable providers expose a sampled-state identity only.
    std::ostringstream bytes;bytes<<"science-state/1 "<<identity_.model_sha256<<' '<<identity_.behavior_sha256<<' '
      <<id_<<' '<<source_<<' '<<identity_.epoch<<' '<<identity_.step_ns<<' '
      <<configured_input_<<' '<<sample.sequence<<' '<<sample.elapsed_ns<<' '<<sample.value<<' '<<sample.uncertainty<<'\n'<<checkpoint;
    return christeye::living::plans::sha256_hex(bytes.str());
  }
public:
  ScientificEngineService(ISupervisedSolver& solver,std::uint32_t id,std::uint32_t source,
    SolverIdentity identity,std::int32_t input=0):solver_(solver),identity_(std::move(identity)),id_(id),source_(source),configured_input_(input){}
  void set_extension(contracts::IServiceExtension* hook)noexcept{extension_=hook;}
  engine::Descriptor describe()const noexcept override{
    engine::Descriptor out{id_,source_,identity_.epoch,engine::Kind::scientific,identity_.model_sha256,
      "NUMERICAL_MODEL_UNCALIBRATED",identity_.checkpoint_schema!=0,identity_.checkpoint_schema!=0};
    const auto* observable=observable_for_unit(identity_.unit);
    if(observable){
      out.capabilities.input_schema=observable->input_schema;
      out.capabilities.metric_schema=observable->metric_schema;
      out.capabilities.summaries[2]=observable->summary;
    }
    out.capabilities.checkpoint_schema=identity_.checkpoint_schema;out.capabilities.clock_domain=source_;
    out.capabilities.tick_ns=identity_.step_ns;out.capabilities.behavior_id=identity_.behavior_sha256;
    return out;
  }
  engine::Status initialize()noexcept override{
    if(initialized_)return solver_.failed()?engine::Status::not_initialized:engine::Status::ok;
    if(!id_||!source_||!identity_.epoch||!observable_for_unit(identity_.unit)||
       configured_input_<0||configured_input_>1000||!solver_.initialize(identity_))return engine::Status::not_initialized;
    initialized_=true;return engine::Status::ok;
  }
  engine::Status latest(engine::Snapshot& out)const noexcept override{
    SolverSample sample{};if(!initialized_||!solver_.latest(sample))return engine::Status::not_initialized;
    std::string checkpoint;
    if(identity_.checkpoint_schema&&!checkpoint_bytes(checkpoint))return engine::Status::io_failure;
    out={};out.id=id_;out.source_epoch=identity_.epoch;out.tick=sample.sequence;out.revision=sample.sequence;
    out.energy=sample.value;
    const auto hash=checkpoint.empty()?snapshot_digest(sample,{}):christeye::living::plans::sha256_hex(checkpoint);
    std::copy(hash.begin(),hash.end(),out.state_sha256.begin());
    return engine::Status::ok;
  }
  engine::Status advance(const engine::StepRequest& request,engine::Snapshot& out)noexcept override{
    SolverSample current{},next{};
    if(!initialized_||!solver_.latest(current))return engine::Status::not_initialized;
    if(request.expected_tick!=current.sequence)return engine::Status::tick_conflict;
    // Scientific process input is explicitly configured; SW5-shaped inputs are
    // rejected rather than reinterpreted as numerical current instructions.
    if(request.inputs.support_proxy||request.inputs.stress_proxy||request.inputs.digital_signal||request.inputs.lesson)return engine::Status::unsupported;
    if(!solver_.step(current.sequence,configured_input_,next))return engine::Status::advance_failed;
    const auto result=latest(out);
    if(result==engine::Status::ok&&extension_)extension_->on_service_event({contracts::ServiceId::scientific_solver,
      contracts::ServicePhase::committed,next.sequence,id_,0,0,identity_.epoch,source_});
    return result;
  }
  engine::Status checkpoint(engine::ICheckpointWriter& writer)const noexcept override{
    if(!initialized_)return engine::Status::not_initialized;
    if(!identity_.checkpoint_schema)return engine::Status::unsupported;
    std::string bytes;return checkpoint_bytes(bytes)&&writer.write(bytes)?engine::Status::ok:engine::Status::io_failure;
  }
  engine::Status restore(engine::ICheckpointReader& reader)noexcept override{
    if(!initialized_||!identity_.checkpoint_schema)return engine::Status::unsupported;
    std::string_view solver;if(!solver_checkpoint(reader.bytes(),solver))return engine::Status::invalid;
    SolverSample restored{};return solver_.restore(solver,restored)?engine::Status::ok:engine::Status::io_failure;
  }
  bool quantity(Quantity& out)const noexcept{
    SolverSample value{};if(!solver_.latest(value))return false;
    out={};out.value=value.value;out.uncertainty=value.uncertainty;out.unit=identity_.unit;out.source=source_;out.epoch=identity_.epoch;
    std::copy(identity_.model_sha256.begin(),identity_.model_sha256.end(),out.model_sha256.begin());return true;
  }
  void cancel()noexcept{solver_.cancel();}
};
} // namespace anaiero::science
