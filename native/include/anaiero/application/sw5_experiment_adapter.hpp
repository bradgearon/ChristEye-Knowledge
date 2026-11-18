#pragma once
// Bounded SW5 experiments reuse the live CellBehaviorService rules. The caller
// supplies an isolated deterministic spatial port; no live observer is attached.
#include "anaiero/contracts/experiment.hpp"
#include "anaiero/services/experiment_service.hpp"
#include "anaiero/services/cell_behavior_service.hpp"
#include "anaiero/services/digital_route_policy_service.hpp"
#include "anaiero/infrastructure/cell_checkpoint.hpp"
#include "anaiero/infrastructure/experiment_definition_file.hpp"
#include "christeye/living/plan_hash.hpp"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <string>
#include <utility>
namespace anaiero::application {
struct ISw5ExperimentPolicy {
  virtual ~ISw5ExperimentPolicy()=default;
  virtual std::string_view behavior_id()const noexcept=0;
  virtual cells::Status apply(cells::CellBehaviorService&,cells::ISpatialPort&,const experiments::Candidate&)noexcept=0;
};
class Sw5GuidanceExperimentPolicy final:public ISw5ExperimentPolicy {
public:
  std::string_view behavior_id()const noexcept override{return "anaiero-sw5/1";}
  cells::Status apply(cells::CellBehaviorService& service,cells::ISpatialPort& spatial,const experiments::Candidate& c)noexcept override {
    if(c.kind!=experiments::PolicyKind::sw5_guidance)return cells::Status::invalid;
    std::uint32_t from=0,to=0;
    const auto a=std::from_chars(c.from.data(),c.from.data()+c.from.size(),from);
    const auto b=std::from_chars(c.to.data(),c.to.data()+c.to.size(),to);
    if(a.ec!=std::errc{}||a.ptr!=c.from.data()+c.from.size()||b.ec!=std::errc{}||
       b.ptr!=c.to.data()+c.to.size())return cells::Status::invalid;
    const auto& rank=c.ranking;
    const optimization::ForkOption option{{rank.id,1,from,to,optimization::RouteKind::digital_signal,
      rank.cost,rank.risk,c.value},c.value,rank.uncertainty};
    optimization::DigitalRoutePolicyService policy(service.atlas(),spatial);
    cells::World changed{};bool existed=false;std::uint16_t old_weight=0;
    if(!policy.apply(service.world(),option,changed,existed,old_weight))return cells::Status::invalid;
    return service.restore(changed);
  }
};
class Sw5ExperimentAdapter final:public experiments::IEvaluator {
  engine::Descriptor descriptor_{};
  cells::Atlas atlas_{};
  cells::World source_{};
  cells::ISpatialPort& spatial_;
  Sw5GuidanceExperimentPolicy guidance_{};
  ISw5ExperimentPolicy* policy_;
  std::string model_,behavior_,checkpoint_,source_hash_,cached_definition_;
  engine::Snapshot baseline_{};
  std::uint32_t baseline_simulations_=0;
  experiments::Code simulate(const experiments::Definition& definition,
      const experiments::Candidate* candidate,engine::Snapshot& output,
      const experiments::ICancellation* cancel)noexcept {
    cells::CellBehaviorService trial(atlas_,spatial_);
    if(trial.restore(source_)!=cells::Status::ok)return experiments::Code::evaluation_failed;
    if(candidate&&policy_->apply(trial,spatial_,*candidate)!=cells::Status::ok)return experiments::Code::evaluation_failed;
    for(std::size_t i=0;i<definition.stage_count;++i){
      const auto& stage=definition.stages[i];const auto& in=stage.inputs;
      for(std::uint16_t tick=0;tick<stage.ticks;++tick){
        if(cancel&&cancel->cancelled())return experiments::Code::cancelled;
        if(trial.step({in.support_proxy,in.stress_proxy,in.digital_signal,in.lesson})!=cells::Status::ok)
          return experiments::Code::evaluation_failed;
      }
    }
    const auto m=cells::metrics(trial.world());
    engine::Snapshot next{};next.id=descriptor_.id;next.source_epoch=descriptor_.source_epoch;
    next.tick=trial.world().tick;next.revision=source_.generation;
    next.participants=m.viable;next.connections=m.linked;next.energy=m.total_energy;next.learning_proxy=m.total_learning;
    const auto sha=christeye::living::plans::sha256_hex(cells::files::serialize_world(trial.world(),atlas_));
    std::copy(sha.begin(),sha.end(),next.state_sha256.begin());
    output=next;return experiments::Code::ok;
  }
public:
  Sw5ExperimentAdapter(engine::Descriptor descriptor,cells::Atlas atlas,cells::World source,
      cells::ISpatialPort& isolated_spatial,ISw5ExperimentPolicy* policy=nullptr):
      descriptor_(descriptor),atlas_(std::move(atlas)),source_(source),spatial_(isolated_spatial),
      policy_(policy?policy:&guidance_),model_(descriptor.model_digest),behavior_(descriptor.capabilities.behavior_id),
      checkpoint_(cells::files::serialize_world(source_,atlas_)),source_hash_(christeye::living::plans::sha256_hex(checkpoint_)) {
    descriptor_.model_digest=model_;descriptor_.capabilities.behavior_id=behavior_;
  }
  std::string_view source_checkpoint()const noexcept{return checkpoint_;}
  std::uint32_t baseline_simulations()const noexcept{return baseline_simulations_;}
  experiments::Code context(const experiments::Definition& d,experiments::EvaluationContext& out)const noexcept override {
    if(!experiments::ExperimentService::valid_definition(d)||!experiments::files::accepts_definition(d))return experiments::Code::invalid_definition;
    if(d.kind!=engine::Kind::sw5||descriptor_.kind!=engine::Kind::sw5||!descriptor_.capabilities.isolated_fork||
       !descriptor_.capabilities.evaluate_candidates||descriptor_.capabilities.input_schema!=1||
       descriptor_.capabilities.metric_schema!=1||descriptor_.capabilities.checkpoint_schema!=1||d.seed!=0)
      return experiments::Code::unsupported;
    if(!descriptor_.source||!descriptor_.source_epoch||!atlas_.synthetic_only||
       model_!=atlas_.model_digest||policy_->behavior_id()!=behavior_||cells::validate_world_invariants(source_)!=cells::Status::ok)
      return experiments::Code::identity_mismatch;
    experiments::EvaluationContext next{};next.engine_id=descriptor_.id;next.source=descriptor_.source;
    next.source_epoch=descriptor_.source_epoch;next.source_revision=source_.generation;next.start_tick=source_.tick;
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
    next.baseline=baseline_;next.source_revision=source_.generation;next.source_state_sha256=source_hash_;next.valid=true;
    if(!experiments::ExperimentService::valid_forecast(d,actual,next))return experiments::Code::identity_mismatch;
    out=std::move(next);return experiments::Code::ok;
  }
};
} // namespace anaiero::application
