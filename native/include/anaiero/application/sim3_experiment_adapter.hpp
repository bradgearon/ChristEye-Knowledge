#pragma once
// In-memory SIM3 forks run the same transition service as durable Runner.
#include "anaiero/contracts/experiment.hpp"
#include "anaiero/services/experiment_service.hpp"
#include "anaiero/services/sim3_transition_service.hpp"
#include "anaiero/infrastructure/experiment_definition_file.hpp"
#include "christeye/living/colony_runner.hpp"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <string>
#include <utility>
namespace anaiero::application {
class Sim3ExperimentAdapter final:public experiments::IEvaluator {
  engine::Descriptor descriptor_{};
  christeye::living::sim3::Configuration configuration_{};
  models::sim3::Snapshot source_{};
  std::string model_,behavior_,checkpoint_,source_hash_,cached_definition_;
  engine::Snapshot baseline_{};
  std::uint32_t baseline_simulations_=0;
  bool apply(models::sim3::ConfigurationData& config,const experiments::Candidate& candidate)const noexcept {
    if(candidate.kind==experiments::PolicyKind::sim3_link_units){
      bool found=false;
      for(auto& link:config.links)if(link.from==candidate.from&&link.to==candidate.to&&
          static_cast<unsigned>(link.channel)==candidate.channel){
        if(found)return false; // ambiguous parallel edges require an explicit model change
        link.units=candidate.value;found=true;
      }
      return found;
    }
    if(candidate.kind!=experiments::PolicyKind::sim3_teach||
       config.processes.size()>=christeye::living::sim3::kMaxProcesses||source_.tick>=10000)return false;
    std::uint32_t subject=0;
    const auto parsed=std::from_chars(candidate.to.data(),candidate.to.data()+candidate.to.size(),subject);
    if(parsed.ec!=std::errc{}||parsed.ptr!=candidate.to.data()+candidate.to.size())return false;
    const auto found=std::find_if(source_.cells.begin(),source_.cells.end(),[&](const auto& cell){
      return cell.colony==candidate.from&&cell.id==subject&&cell.alive;
    });
    if(found==source_.cells.end())return false;
    config.processes.push_back({candidate.from,models::sim3::ProcessKind::teach,subject,candidate.value,
                               static_cast<std::uint32_t>(source_.tick+1)});
    return true;
  }
  experiments::Code simulate(const experiments::Definition& d,const experiments::Candidate* candidate,
      engine::Snapshot& output,const experiments::ICancellation* cancel)const noexcept {
    models::sim3::ConfigurationData config=configuration_;auto state=source_;
    if(candidate&&!apply(config,*candidate))return experiments::Code::evaluation_failed;
    simulation::Sim3TransitionService transitions;
    for(std::uint16_t tick=0;tick<d.optimization.horizon;++tick){
      if(cancel&&cancel->cancelled())return experiments::Code::cancelled;
      const auto result=transitions.advance(config,state);
      if(result!=models::sim3::Result::ok)return result==models::sim3::Result::capacity?
        experiments::Code::capacity:experiments::Code::evaluation_failed;
    }
    engine::Snapshot next{};next.id=descriptor_.id;next.source_epoch=descriptor_.source_epoch;
    next.tick=state.tick;next.revision=source_.tick;
    next.participants=static_cast<std::uint32_t>(std::count_if(state.cells.begin(),state.cells.end(),
      [](const auto& cell){return cell.alive;}));
    next.connections=static_cast<std::uint32_t>(config.links.size());
    for(const auto& cell:state.cells){next.energy+=cell.energy;next.learning_proxy+=cell.learning;}
    const auto sha=christeye::living::plans::sha256_hex(christeye::living::sim3::encode_snapshot(state));
    std::copy(sha.begin(),sha.end(),next.state_sha256.begin());output=next;return experiments::Code::ok;
  }
public:
  Sim3ExperimentAdapter(engine::Descriptor descriptor,christeye::living::sim3::Configuration configuration,
      models::sim3::Snapshot source):descriptor_(descriptor),configuration_(std::move(configuration)),source_(std::move(source)),
      model_(descriptor.model_digest),behavior_(descriptor.capabilities.behavior_id),
      checkpoint_(christeye::living::sim3::encode_snapshot(source_)),source_hash_(christeye::living::plans::sha256_hex(checkpoint_)) {
    descriptor_.model_digest=model_;descriptor_.capabilities.behavior_id=behavior_;
  }
  std::string_view source_checkpoint()const noexcept{return checkpoint_;}
  std::uint32_t baseline_simulations()const noexcept{return baseline_simulations_;}
  experiments::Code context(const experiments::Definition& d,experiments::EvaluationContext& out)const noexcept override {
    if(!experiments::ExperimentService::valid_definition(d)||!experiments::files::accepts_definition(d))return experiments::Code::invalid_definition;
    if(d.kind!=engine::Kind::sim3||descriptor_.kind!=engine::Kind::sim3||!descriptor_.capabilities.isolated_fork||
       !descriptor_.capabilities.evaluate_candidates||descriptor_.capabilities.input_schema!=2||
       descriptor_.capabilities.metric_schema!=2||descriptor_.capabilities.checkpoint_schema!=1||d.seed!=0)
      return experiments::Code::unsupported;
    if(!descriptor_.source||!descriptor_.source_epoch||model_!=configuration_.hash||source_.config_hash!=model_)
      return experiments::Code::identity_mismatch;
    if(source_.tick>10000||d.optimization.horizon>10000-source_.tick)return experiments::Code::capacity;
    models::sim3::Snapshot validated{};
    if(christeye::living::sim3::decode_snapshot(checkpoint_,configuration_,validated)!=models::sim3::Result::ok)
      return experiments::Code::identity_mismatch;
    experiments::EvaluationContext next{};next.engine_id=descriptor_.id;next.source=descriptor_.source;
    next.source_epoch=descriptor_.source_epoch;next.source_revision=source_.tick;next.start_tick=source_.tick;
    next.source_state_sha256=source_hash_;next.model_sha256=model_;next.behavior_id=behavior_;
    next.metric_schema=descriptor_.capabilities.metric_schema;next.clock_domain=descriptor_.capabilities.clock_domain;
    next.tick_ns=descriptor_.capabilities.tick_ns;next.seed=d.seed;next.input_schedule_sha256=d.input_schedule_sha256;
    if(!experiments::ExperimentService::matches(d,next))return experiments::Code::identity_mismatch;
    out=std::move(next);return experiments::Code::ok;
  }
  experiments::Code evaluate(const experiments::Definition& d,const experiments::EvaluationContext& expected,
      std::size_t candidate,engine::Forecast& out,const experiments::ICancellation* cancel)noexcept override {
    out={};experiments::EvaluationContext actual{};const auto status=context(d,actual);
    if(status!=experiments::Code::ok)return status;
    if(!experiments::ExperimentService::same_context(actual,expected))return experiments::Code::identity_mismatch;
    if(candidate>=d.candidate_count)return experiments::Code::invalid_definition;
    if(cancel&&cancel->cancelled())return experiments::Code::cancelled;
    if(cached_definition_!=d.sha256){
      engine::Snapshot baseline{};const auto result=simulate(d,nullptr,baseline,cancel);
      if(result!=experiments::Code::ok)return result;
      baseline_=baseline;cached_definition_=d.sha256;++baseline_simulations_;
    }
    engine::Forecast next{};const auto result=simulate(d,&d.candidates[candidate],next.trial,cancel);
    if(result!=experiments::Code::ok)return result;
    next.baseline=baseline_;next.source_revision=source_.tick;next.source_state_sha256=source_hash_;next.valid=true;
    if(!experiments::ExperimentService::valid_forecast(d,actual,next))return experiments::Code::identity_mismatch;
    out=std::move(next);return experiments::Code::ok;
  }
};
} // namespace anaiero::application
