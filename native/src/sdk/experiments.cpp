#include "anaiero/sdk/experiments.hpp"
#include "validation.hpp"
#include <exception>
#include <map>

namespace anaiero::sdk {
namespace {
Result final_metrics(const std::vector<Observation>& observations,std::map<std::string,Observation>& output) {
  if(observations.empty()||observations.size()>4096)return {Code::invalid,"comparison requires bounded observations"};
  for(const auto& observation:observations) {
    if(!valid_token(observation.metric)||!valid_token(observation.schema)||!valid_token(observation.quantity)||!valid_token(observation.unit))
      return {Code::invalid,"comparison observation semantics"};
    auto at=output.find(observation.metric);
    if(at!=output.end()&&(at->second.schema!=observation.schema||at->second.quantity!=observation.quantity||
        at->second.unit!=observation.unit||observation.tick<at->second.tick))
      return {Code::conflict,"trial changed metric semantics or logical clock"};
    output[observation.metric]=observation;
  }
  return {};
}
Result comparison(const std::vector<Observation>& baseline,const std::vector<Observation>& candidate,
    std::vector<MetricComparison>& output) {
  std::map<std::string,Observation> left,right;
  auto result=final_metrics(baseline,left);if(!result)return result;
  result=final_metrics(candidate,right);if(!result)return result;
  if(left.size()!=right.size())return {Code::conflict,"candidate and baseline expose different metric sets"};
  std::vector<MetricComparison> next;
  for(const auto& [metric,value]:left) {
    const auto at=right.find(metric);
    if(at==right.end()||value.schema!=at->second.schema||value.quantity!=at->second.quantity||
        value.unit!=at->second.unit||value.tick!=at->second.tick)
      return {Code::conflict,"candidate metric schema, quantity, unit or clock differs from baseline"};
    next.push_back({value,at->second});
  }
  output=std::move(next);return {};
}
Bytes strategy_identity(const ScoreStrategyIdentity& identity) {
  return pack({identity.id,identity.implementation_sha256,encode_envelope(identity.configuration)});
}
bool valid_strategy(const ScoreStrategyIdentity& identity) {
  return valid_token(identity.id)&&valid_digest(identity.implementation_sha256)&&
    valid_token(identity.configuration.schema)&&identity.configuration.payload.size()<=65536;
}
Result scored(const IScoreStrategy& strategy,const ScoreStrategyIdentity& identity,
    const std::vector<Observation>& baseline,const std::vector<Observation>& candidate,Score& output) {
  std::vector<MetricComparison> metrics;auto result=comparison(baseline,candidate,metrics);if(!result)return result;
  try {
    if(strategy_identity(strategy.identity())!=strategy_identity(identity))return {Code::conflict,"score strategy changed identity/configuration"};
    Score next;result=strategy.score(metrics,next);if(!result)return result;
    if(next.rationale.size()>512)return {Code::capacity,"score rationale bound"};
    if(strategy_identity(strategy.identity())!=strategy_identity(identity))return {Code::conflict,"score strategy changed during evaluation"};
    output=std::move(next);return {};
  }catch(...){return {Code::io_error,"score strategy threw"};}
}
}
Result ExperimentService::evaluate(IEngine& source,const TrialProgram& program,TrialResult& output)const {
  if(!valid_token(program.id,100)||program.inputs.size()>128)
    return {Code::invalid,"trial identity or step bound"};
  auto* capability=source.trials();
  if(!capability)return {Code::unsupported,"engine has no isolated trial implementation"};
  std::size_t input_bytes=program.has_change?program.change.payload.size():0;
  for(const auto& input:program.inputs) {
    if(input.payload.size()>4U*1024U*1024U||input_bytes>16U*1024U*1024U-input.payload.size())
      return {Code::capacity,"trial aggregate input byte bound"};
    input_bytes+=input.payload.size();
  }
  const auto original=source.state();const auto identity=source.identity();
  try {
    std::unique_ptr<IEngine> isolated;auto result=capability->fork(isolated);if(!result)return result;
    if(!isolated||isolated->state()!=original||isolated->identity()!=identity||source.state()!=original)
      return {Code::corrupt,"trial fork changed its source"};
    TrialResult next;next.id=program.id;
    next.receipt_tip=sha256(pack({"trial/1",program.id,encode_identity(identity),encode_state(original)}));
    std::size_t ordinal=0;
    auto execute=[&](const Envelope& input,bool change)->Result {
      if(input.payload.size()>Limits{}.max_payload_bytes||!valid_token(input.schema))
        return {Code::capacity,"trial input envelope bounds"};
      const auto before=isolated->state();
      StepRequest request{program.id+"-"+std::to_string(ordinal++),before.revision,input};
      PreparedTransition prepared;
      Result prepared_result;
      if(change) {
        auto* changes=isolated->changes();
        if(!changes)return {Code::unsupported,"engine has no candidate change implementation"};
        prepared_result=changes->prepare_change(request,prepared);
      }else prepared_result=isolated->prepare_step(request,prepared);
      if(isolated->state()!=before||source.state()!=original)
        return {Code::corrupt,"trial preparation mutated visible source state"};
      if(!prepared_result)return prepared_result;
      auto valid=detail::validate_transition(identity,before,request,prepared,nullptr,change);if(!valid)return valid;
      if(next.observations.size()+prepared.observations.size()>4096)
        return {Code::capacity,"trial observation bound"};
      auto installed=isolated->install(prepared.target);if(!installed)return installed;
      if(isolated->state()!=prepared.target)return {Code::corrupt,"trial install mismatch"};
      next.observations.insert(next.observations.end(),prepared.observations.begin(),prepared.observations.end());
      next.receipt_tip=sha256(pack({"trial-receipt/1",next.receipt_tip,encode_transition(prepared)}));
      return {};
    };
    if(program.has_change){result=execute(program.change,true);if(!result)return result;}
    for(const auto& input:program.inputs){result=execute(input,false);if(!result)return result;}
    if(source.state()!=original)return {Code::corrupt,"trial changed source state"};
    next.final_state=isolated->state();output=std::move(next);return {};
  }catch(const std::exception& error){return {Code::io_error,std::string("trial module failure: ")+error.what()};}
  catch(...){return {Code::io_error,"trial module failure"};}
}
Result ExperimentService::replay_equivalence(IEngine& source,const TrialProgram& program,
    const EngineState& expected,TrialResult& output)const {
  TrialResult result;auto evaluated=evaluate(source,program,result);if(!evaluated)return evaluated;
  if(result.final_state!=expected)return {Code::conflict,"isolated replay differs from expected committed state"};
  output=std::move(result);return {};
}
Result ExperimentService::select(IEngine& source,const ExperimentRequest& request,const IScoreStrategy& strategy,
    ExperimentResult& output)const {
  if(!valid_token(request.id,80)||request.inputs.empty()||request.inputs.size()>128||
      request.candidates.empty()||request.candidates.size()>8||!valid_digest(request.expected_state_sha256))
    return {Code::invalid,"experiment identity, horizon or candidate bound"};
  const auto original=source.state();const auto identity=source.identity();
  if(request.expected_revision!=original.revision||request.expected_state_sha256!=state_digest(original))
    return {Code::conflict,"experiment source checkpoint/revision changed"};
  if(!source.trials()||!source.changes())return {Code::unsupported,"engine lacks isolated change trials"};
  ScoreStrategyIdentity score_identity;
  try{score_identity=strategy.identity();}catch(...){return {Code::io_error,"score strategy identity threw"};}
  if(!valid_strategy(score_identity))return {Code::invalid,"score strategy identity/configuration pin"};
  std::vector<std::string> program{"experiment/1",request.id,encode_identity(identity),
    std::to_string(request.expected_revision),request.expected_state_sha256,strategy_identity(score_identity)};
  std::size_t bytes=0;
  for(const auto& input:request.inputs) {
    if(!valid_token(input.schema)||input.payload.size()>4U*1024U*1024U||bytes>16U*1024U*1024U-input.payload.size())
      return {Code::capacity,"experiment aggregate input bound"};
    bytes+=input.payload.size();program.push_back(encode_envelope(input));
  }
  program.push_back("candidates");
  for(std::size_t i=0;i<request.candidates.size();++i) {
    const auto& candidate=request.candidates[i];
    if(!valid_token(candidate.id,80)||candidate.id=="baseline"||!valid_token(candidate.change.schema)||
        candidate.change.payload.size()>4U*1024U*1024U||bytes>16U*1024U*1024U-candidate.change.payload.size())
      return {Code::invalid,"experiment candidate identity/schema/byte bound"};
    for(std::size_t j=0;j<i;++j)if(request.candidates[j].id==candidate.id)return {Code::conflict,"duplicate experiment candidate"};
    bytes+=candidate.change.payload.size();program.push_back(pack({candidate.id,encode_envelope(candidate.change)}));
  }
  ExperimentResult next;next.source=identity;next.source_revision=original.revision;next.source_state_sha256=request.expected_state_sha256;
  next.program_sha256=sha256(pack(program));next.strategy=score_identity;
  auto unchanged=[&]{return source.identity()==identity&&source.state()==original;};
  auto notify=[&](std::string operation,const Result& status)noexcept {
    if(observer_)try{(void)observer_->enqueue({ObserverEventKind::trial_completed,identity,std::move(operation),
      original.revision,original.tick,0,original.phase,status.code});}catch(...){}
  };
  auto result=evaluate(source,{request.id+"-baseline",request.inputs,{},false},next.baseline);
  if(!unchanged())return {Code::conflict,"experiment source advanced during baseline"};
  if(!result)return result;
  notify(request.id+"-baseline",result);
  result=scored(strategy,score_identity,next.baseline.observations,next.baseline.observations,next.baseline_score);
  if(!result)return result;
  if(!unchanged())return {Code::conflict,"experiment source advanced during scoring"};
  Score best=next.baseline_score;
  for(std::size_t i=0;i<request.candidates.size();++i) {
    const auto& candidate=request.candidates[i];CandidateResult evaluated;evaluated.id=candidate.id;
    evaluated.status=evaluate(source,{request.id+"-candidate-"+std::to_string(i),request.inputs,candidate.change,true},evaluated.trial);
    if(!unchanged())return {Code::conflict,"experiment source advanced during candidate"};
    if(evaluated.status) {
      evaluated.status=scored(strategy,score_identity,next.baseline.observations,evaluated.trial.observations,evaluated.score);
      if(!unchanged())return {Code::conflict,"experiment source advanced during candidate scoring"};
      if(evaluated.status&&evaluated.score.eligible&&(!best.eligible||evaluated.score.value>best.value||
          (evaluated.score.value==best.value&&!next.chosen_candidate.empty()&&candidate.id<next.chosen_candidate))) {
        next.chosen_candidate=candidate.id;best=evaluated.score;
      }
    }
    notify(request.id+"-candidate-"+std::to_string(i),evaluated.status);
    next.candidates.push_back(std::move(evaluated));
  }
  output=std::move(next);return {};
}
Result ExperimentService::compare_outcome(const ExperimentResult& frozen,const OutcomeEvidence& observed,
    const IScoreStrategy& strategy,OutcomeReview& output)const {
  if(!valid_identity(frozen.source)||frozen.source!=observed.identity||!valid_state(observed.state)||
      !valid_digest(observed.receipt_tip)||!valid_digest(frozen.program_sha256)||!valid_digest(frozen.source_state_sha256)||
      observed.state.revision<frozen.source_revision||!valid_strategy(frozen.strategy))
    return {Code::conflict,"outcome identity, state or retained evidence witness mismatch"};
  const TrialResult* expected=&frozen.baseline;const Score* selected=&frozen.baseline_score;
  if(!frozen.chosen_candidate.empty()) {
    const auto at=std::find_if(frozen.candidates.begin(),frozen.candidates.end(),[&](const auto& candidate){return candidate.id==frozen.chosen_candidate;});
    if(at==frozen.candidates.end()||!at->status||!at->score.eligible)return {Code::corrupt,"selected experiment candidate is unavailable"};
    expected=&at->trial;selected=&at->score;
  }
  if(observed.state.tick!=expected->final_state.tick)return {Code::conflict,"outcome uses a different observation horizon"};
  for(const auto& observation:observed.observations)
    if(observation.tick>observed.state.tick)return {Code::corrupt,"outcome observation exceeds canonical state clock"};
  OutcomeReview next;auto result=scored(strategy,frozen.strategy,frozen.baseline.observations,observed.observations,next.measured_score);
  if(!result)return result;
  next.meets_selected_score=next.measured_score.eligible&&next.measured_score.value>=selected->value;
  next.exact_selected_state=observed.state==expected->final_state;
  next.expected_state_sha256=state_digest(expected->final_state);next.observed_state_sha256=state_digest(observed.state);
  next.observed_receipt_tip=observed.receipt_tip;output=std::move(next);return {};
}
}
