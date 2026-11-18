#pragma once
// Bounded candidate selection on training data followed by one frozen holdout
// evaluation. Passing a declared reference is not biological validation.
#include "anaiero/contracts/scientific_calibration.hpp"
#include "anaiero/services/environment_compartment_service.hpp"
#include "christeye/living/plan_hash.hpp"
#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <utility>
namespace anaiero::science::calibration {
class NativeEnvironmentTrial final:public IEnvironmentTrial {
  EnvironmentCompartmentService model_;
  std::string behavior_=christeye::living::plans::sha256_hex(kEnvironmentAnalyticBehavior);
public:
  std::string_view behavior_sha256()const noexcept override{return behavior_;}
  bool begin(const EnvironmentCompartmentModel& model,std::uint64_t step_ns)noexcept override{
    return model_.initialize(model,step_ns);
  }
  bool advance(std::uint64_t sequence,std::int32_t input,std::int64_t& value)noexcept override{
    if(!model_.advance(sequence,input))return false;
    value=model_.concentration_micromolar();return true;
  }
};
class AbsoluteFidelityPolicy final:public IFidelityPolicy {
public:
  std::string_view id()const noexcept override{return "raw-absolute-and-rmse/1";}
  bool accepts(const ErrorSummary& summary,const FidelityCriteria& criteria)const noexcept override{
    if(!summary.count||summary.count>kMaxSamples||criteria.max_absolute_error_micromolar>1000000||
       criteria.max_rmse_micromolar>1000000)return false;
    return summary.max_absolute_error_micromolar<=criteria.max_absolute_error_micromolar&&
      summary.sum_squared_error<=criteria.max_rmse_micromolar*criteria.max_rmse_micromolar*summary.count;
  }
};
inline double root_mean_squared_error(const ErrorSummary& value)noexcept{
  return value.count?std::sqrt(static_cast<double>(value.sum_squared_error)/static_cast<double>(value.count)):0.0;
}
class EnvironmentCalibrationService final {
  IEnvironmentTrial& trial_;const IFidelityPolicy& fidelity_;
  IObserver* observer_=nullptr;
  bool running_=false;
  static bool digest(std::string_view value)noexcept{
    if(value.size()!=64)return false;
    for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
  }
  static bool identifier(std::string_view value)noexcept{
    if(value.empty()||value.size()>80)return false;
    for(char c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))return false;
    return true;
  }
  static bool same_model(const EnvironmentCompartmentModel& a,const EnvironmentCompartmentModel& b)noexcept{
    return a.initial_micromolar==b.initial_micromolar&&a.inlet_max_micromolar==b.inlet_max_micromolar&&
      a.exchange_tau_ns==b.exchange_tau_ns&&a.consumption_ppm_per_second==b.consumption_ppm_per_second;
  }
  static bool reference_valid(const Reference& data)noexcept{
    const auto& p=data.provenance;
    if(!digest(data.sha256)||data.unit!=Unit::micromolar||!data.step_ns||data.step_ns>1000000000ULL||
       data.initial_micromolar<0||data.initial_micromolar>1000000||
       data.initial_uncertainty_micromolar<0||data.initial_uncertainty_micromolar>1000000||
       data.samples.empty()||data.samples.size()>kMaxSamples||!p.source||!p.clock_domain||!p.epoch||
       !identifier(p.run_id)||!digest(p.producer_sha256)||!digest(p.protocol_sha256)||!digest(p.raw_sha256)||
       (p.origin!=ReferenceOrigin::synthetic_reference&&p.origin!=ReferenceOrigin::external_unverified_readonly))return false;
    for(std::size_t i=0;i<data.samples.size();++i){
      const auto& sample=data.samples[i];
      const auto sequence=static_cast<std::uint64_t>(i)+1;
      if(sample.sequence!=sequence||sample.acquired_ns!=sequence*data.step_ns||sample.inlet_input<0||sample.inlet_input>1000||
         sample.concentration_micromolar<0||sample.concentration_micromolar>1000000||
         sample.uncertainty_micromolar<0||sample.uncertainty_micromolar>1000000)return false;
    }
    return true;
  }
  static Code validate(const Plan& plan,const Reference& training,const Reference& holdout)noexcept{
    if(!identifier(plan.name)||!digest(plan.sha256)||!digest(plan.training_sha256)||!digest(plan.holdout_sha256)||
       plan.candidates.empty()||plan.candidates.size()>kMaxCandidates||
       plan.criteria.max_absolute_error_micromolar>1000000||plan.criteria.max_rmse_micromolar>1000000)
      return Code::invalid_plan;
    if(!reference_valid(training)||!reference_valid(holdout)||training.sha256!=plan.training_sha256||
       holdout.sha256!=plan.holdout_sha256)return Code::invalid_reference;
    // A source+epoch identifies a physical/logical run. The same source may be
    // reused in a new epoch, but relabeling the same raw bytes is not a split.
    if(training.sha256==holdout.sha256||training.provenance.raw_sha256==holdout.provenance.raw_sha256||
       training.provenance.run_id==holdout.provenance.run_id||
       (training.provenance.source==holdout.provenance.source&&training.provenance.epoch==holdout.provenance.epoch))
      return Code::contaminated_split;
    if(training.provenance.origin!=holdout.provenance.origin||training.step_ns!=holdout.step_ns||
       training.initial_micromolar!=holdout.initial_micromolar||
       training.provenance.protocol_sha256!=holdout.provenance.protocol_sha256)
      return Code::invalid_reference;
    for(std::size_t i=0;i<plan.candidates.size();++i){
      const auto& candidate=plan.candidates[i];
      if(!identifier(candidate.id)||!digest(candidate.model_sha256)||!EnvironmentCompartmentService::valid_model(candidate.model)||
         candidate.model.initial_micromolar!=training.initial_micromolar)
        return Code::invalid_plan;
      for(std::size_t j=0;j<i;++j)
        if(candidate.id==plan.candidates[j].id||candidate.model_sha256==plan.candidates[j].model_sha256||
           same_model(candidate.model,plan.candidates[j].model))return Code::invalid_plan;
    }
    return Code::ok;
  }
  void event(Phase phase,std::size_t index,std::uint64_t samples,Code code=Code::ok)noexcept{
    if(observer_)observer_->on_calibration_event({phase,index,samples,code});
  }
  Code evaluate(const Candidate& candidate,const Reference& reference,ErrorSummary& score,
                const ICancellation* cancellation)noexcept{
    score={};
    if(cancellation&&cancellation->cancelled())return Code::cancelled;
    if(!trial_.begin(candidate.model,reference.step_ns))return Code::model_failure;
    for(const auto& sample:reference.samples){
      if(cancellation&&cancellation->cancelled())return Code::cancelled;
      std::int64_t predicted=0;
      if(!trial_.advance(sample.sequence,sample.inlet_input,predicted)||predicted<0||predicted>1000000)return Code::model_failure;
      const auto signed_error=predicted-sample.concentration_micromolar;
      const auto error=static_cast<std::uint64_t>(signed_error<0?-signed_error:signed_error);
      ++score.count;score.sum_squared_error+=error*error;
      score.max_absolute_error_micromolar=std::max(score.max_absolute_error_micromolar,error);
      score.max_reference_uncertainty_micromolar=std::max(score.max_reference_uncertainty_micromolar,
        static_cast<std::uint64_t>(sample.uncertainty_micromolar));
    }
    return Code::ok;
  }
public:
  EnvironmentCalibrationService(IEnvironmentTrial& trial,const IFidelityPolicy& fidelity):trial_(trial),fidelity_(fidelity){}
  void set_observer(IObserver* observer)noexcept{observer_=observer;}
  Code run(const Plan& plan,const Reference& training,const Reference& holdout,Report& output,
           const ICancellation* cancellation=nullptr){
    if(running_)return Code::reentrant;
    running_=true;struct Reset{bool& running;~Reset(){running=false;}} reset{running_};
    Report report{};
    const auto result=validate(plan,training,holdout);
    if(result!=Code::ok){report.code=result;event(Phase::rejected,0,0,result);output=std::move(report);return result;}
    if(!digest(trial_.behavior_sha256())||fidelity_.id().empty()||fidelity_.id().size()>80){
      report.code=Code::invalid_policy;output=std::move(report);return Code::invalid_policy;
    }
    report.plan_name=plan.name;report.plan_sha256=plan.sha256;report.training_sha256=training.sha256;
    report.holdout_sha256=holdout.sha256;report.training_provenance=training.provenance;
    report.holdout_provenance=holdout.provenance;report.criteria=plan.criteria;
    report.initial_micromolar=training.initial_micromolar;report.step_ns=training.step_ns;
    report.training_initial_uncertainty_micromolar=training.initial_uncertainty_micromolar;
    report.holdout_initial_uncertainty_micromolar=holdout.initial_uncertainty_micromolar;
    report.behavior_sha256=trial_.behavior_sha256();report.selection_policy="minimum-training-squared-error/1;ties=plan-order";
    report.fidelity_policy=fidelity_.id();report.candidates.reserve(plan.candidates.size());
    event(Phase::validated,0,0);
    for(std::size_t i=0;i<plan.candidates.size();++i){
      const auto& candidate=plan.candidates[i];CandidateResult measured{candidate.id,candidate.model_sha256,{}};
      report.code=evaluate(candidate,training,measured.training,cancellation);
      if(report.code!=Code::ok){event(Phase::rejected,i,measured.training.count,report.code);output=std::move(report);return output.code;}
      report.candidates.push_back(std::move(measured));
      if(i&&report.candidates[i].training.sum_squared_error<report.candidates[report.selected_index].training.sum_squared_error)
        report.selected_index=i;
      event(Phase::training_complete,i,report.candidates[i].training.count);
    }
    // The selected index is frozen before holdout values are passed to the
    // trial. There is no fallback candidate search after a holdout failure.
    report.winner_frozen=true;
    report.frozen_candidate_id=plan.candidates[report.selected_index].id;
    report.frozen_model_sha256=plan.candidates[report.selected_index].model_sha256;
    report.training_passed=fidelity_.accepts(report.candidates[report.selected_index].training,plan.criteria);
    event(Phase::winner_frozen,report.selected_index,0);
    report.code=evaluate(plan.candidates[report.selected_index],holdout,report.holdout,cancellation);
    if(report.code==Code::ok){
      report.holdout_passed=fidelity_.accepts(report.holdout,plan.criteria);
      report.passed=report.training_passed&&report.holdout_passed;
      event(Phase::holdout_complete,report.selected_index,report.holdout.count);
    }else event(Phase::rejected,report.selected_index,report.holdout.count,report.code);
    output=std::move(report);return output.code;
  }
};
} // namespace anaiero::science::calibration
