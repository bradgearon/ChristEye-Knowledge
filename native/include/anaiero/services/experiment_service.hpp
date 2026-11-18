#pragma once
// Serial resumable experiment decisions. Evaluators own isolated digital forks;
// the live session remains the only authority that can apply their results.
#include "anaiero/contracts/experiment.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include "anaiero/services/engine_optimization_service.hpp"
#include "christeye/living/plan_hash.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <sstream>
#include <utility>
namespace anaiero::experiments {
class ExperimentService final {
  IRecordStore& store_;
  IEvaluator& evaluator_;
  Record record_{};
  Code code_=Code::ok;
  bool loaded_=false;
  bool persistence_fault_=false;
  contracts::IServiceExtension* extension_=nullptr;
  static bool digest(std::string_view s)noexcept {
    return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){
      return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
  }
  static bool identity(std::string_view s)noexcept {
    return !s.empty()&&s.size()<=64&&std::all_of(s.begin(),s.end(),[](char c){
      return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.';});
  }
  static std::int64_t metric(const engine::Snapshot& s,engine::ObjectiveMetric m)noexcept {
    switch(m){case engine::ObjectiveMetric::participants:return s.participants;
      case engine::ObjectiveMetric::connections:return s.connections;
      case engine::ObjectiveMetric::energy:return s.energy;
      case engine::ObjectiveMetric::learning_proxy:return s.learning_proxy;}
    return 0;
  }
  static std::int64_t distance(std::int64_t a,std::int64_t b)noexcept{return a>b?a-b:b-a;}
  bool save()noexcept {
    if(!store_.save(record_)){persistence_fault_=true;code_=Code::persistence_failed;return false;}
    if(extension_){
      std::uint64_t operation=0;constexpr std::string_view prefix="experiment-";
      if(record_.operation_id.starts_with(prefix)){
        const auto suffix=std::string_view(record_.operation_id).substr(prefix.size());
        const auto parsed=std::from_chars(suffix.data(),suffix.data()+suffix.size(),operation);
        if(parsed.ec!=std::errc{}||parsed.ptr!=suffix.data()+suffix.size())operation=0;
      }
      extension_->on_service_event({contracts::ServiceId::experiment,
        contracts::ServicePhase::committed,record_.revision,record_.definition.engine_id,
        static_cast<std::int32_t>(record_.state),operation,record_.context.source_epoch,record_.context.source});
    }
    return true;
  }
  static engine::Selection selection_for(const Record& record)noexcept {
    struct Cached final:engine::IScenarioEvaluator {
      const Record& record;
      explicit Cached(const Record& r):record(r){}
      bool evaluate(const engine::OptimizationPlan&,const engine::Candidate& c,engine::Forecast& out)noexcept override {
        for(std::size_t i=0;i<record.definition.candidate_count;++i)
          if(record.trials[i].candidate.id==c.id&&record.trials[i].state==TrialState::completed){
            out=record.trials[i].forecast;return true;
          }
        return false;
      }
    } evaluator(record);
    std::array<engine::Candidate,kMaxCandidates> candidates{};
    for(std::size_t i=0;i<record.definition.candidate_count;++i)candidates[i]=record.definition.candidates[i].ranking;
    engine::EngineOptimizationService optimizer(evaluator);
    engine::Selection selection{};
    optimizer.analyze(record.definition.optimization,
      std::span<const engine::Candidate>(candidates.data(),record.definition.candidate_count),selection);
    return selection; // a valid experiment can complete with no approved improvement
  }
  static bool same_candidate(const engine::Candidate& a,const engine::Candidate& b)noexcept {
    return a.id==b.id&&a.policy_token==b.policy_token&&a.cost==b.cost&&a.risk==b.risk&&a.uncertainty==b.uncertainty;
  }
  static bool same_snapshot(const engine::Snapshot& a,const engine::Snapshot& b)noexcept {
    return a.id==b.id&&a.source_epoch==b.source_epoch&&a.tick==b.tick&&a.revision==b.revision&&
      a.participants==b.participants&&a.connections==b.connections&&a.energy==b.energy&&
      a.learning_proxy==b.learning_proxy&&a.state_sha256==b.state_sha256;
  }
public:
  ExperimentService(IRecordStore& store,IEvaluator& evaluator):store_(store),evaluator_(evaluator){}
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  const Record& record()const noexcept{return record_;}
  Code code()const noexcept{return code_;}
  bool busy()const noexcept{return loaded_&&!persistence_fault_&&(record_.state==RunState::prepared||record_.state==RunState::running);}
  static bool valid_definition(const Definition& d)noexcept {
    if(!identity(d.name)||!digest(d.sha256)||!digest(d.model_sha256)||!digest(d.input_schedule_sha256)||
       d.behavior_id.empty()||d.behavior_id.size()>96||!d.engine_id||!d.metric_schema||!d.clock_domain||
       (d.kind!=engine::Kind::sw5&&d.kind!=engine::Kind::sim3)||
       !d.candidate_count||d.candidate_count>kMaxCandidates||!d.stage_count||d.stage_count>kMaxStages||
       d.artifact_count>kMaxArtifacts||!d.optimization.horizon||d.optimization.horizon>128||
       !d.optimization.goal_count||d.optimization.goal_count>d.optimization.objectives.size()||
       d.optimization.minimum_score<1||d.outcome_tolerance<0||d.outcome_tolerance>1000000000||
       (d.outcome_ticks&&d.outcome_ticks!=d.optimization.horizon)||
       (d.automatic_revert&&!d.outcome_ticks)||
       (d.kind==engine::Kind::sim3&&(d.automatic_apply||d.automatic_revert)))return false;
    std::array<bool,5> metrics{};
    for(std::size_t i=0;i<d.optimization.goal_count;++i){
      const auto& o=d.optimization.objectives[i];const auto m=static_cast<unsigned>(o.metric);
      if(!m||m>=metrics.size()||metrics[m]||!o.importance||o.importance>1000||
         o.ideal< -1000000000||o.ideal>1000000000)return false;
      metrics[m]=true;
    }
    std::uint32_t ticks=0;
    for(std::size_t i=0;i<d.stage_count;++i){
      const auto& s=d.stages[i];if(!identity(s.id)||!s.ticks||s.ticks>128)return false;
      for(std::size_t j=0;j<i;++j)if(s.id==d.stages[j].id)return false;
      const auto& p=s.inputs;
      if(p.support_proxy<0||p.support_proxy>1000||p.stress_proxy<0||p.stress_proxy>1000||
         p.digital_signal<0||p.digital_signal>1000||
         (d.kind==engine::Kind::sim3&&(p.support_proxy||p.stress_proxy||p.digital_signal||p.lesson)))return false;
      ticks+=s.ticks;
    }
    if(ticks!=d.optimization.horizon)return false;
    for(std::size_t i=0;i<d.candidate_count;++i){
      const auto& c=d.candidates[i];const auto& r=c.ranking;
      if(!r.id||!r.policy_token||r.cost>1000||r.risk>1000||r.uncertainty>1000||
         !identity(c.from)||!identity(c.to)||c.from.size()>56||c.to.size()>56)return false;
      for(std::size_t j=0;j<i;++j)if(r.id==d.candidates[j].ranking.id||r.policy_token==d.candidates[j].ranking.policy_token)return false;
      if(d.kind==engine::Kind::sw5){
        if(c.kind!=PolicyKind::sw5_guidance||!c.value||c.value>1000||c.channel||c.from==c.to)return false;
        for(const auto* value:{&c.from,&c.to}){std::uint32_t id=0;
          const auto [end,ec]=std::from_chars(value->data(),value->data()+value->size(),id);
          if(ec!=std::errc{}||end!=value->data()+value->size()||!id)return false;
        }
      }else if(c.kind==PolicyKind::sim3_link_units){
        if(c.from==c.to||c.channel<1||c.channel>3||c.value>100)return false;
      }else if(c.kind==PolicyKind::sim3_teach){
        if(c.channel||!c.value||c.value>100)return false;
        std::uint32_t id=0;const auto [end,ec]=std::from_chars(c.to.data(),c.to.data()+c.to.size(),id);
        if(ec!=std::errc{}||end!=c.to.data()+c.to.size()||!id)return false;
      }else return false;
    }
    for(std::size_t i=0;i<d.artifact_count;++i){
      const auto& a=d.artifacts[i];if(!identity(a.id)||!digest(a.sha256)||a.relative_path.empty()||
        a.relative_path.size()>240)return false;
      if(a.role!="digital_crystal"&&a.role!="digital_learning"&&a.role!="rna_observation"&&
         a.role!="support_profile"&&a.role!="visual"&&a.role!="physical_crystal_candidate")return false;
      for(std::size_t j=0;j<i;++j)if(a.id==d.artifacts[j].id)return false;
    }
    const bool linked=!d.learning_plan_path.empty();
    if(linked!=!d.learning_base_path.empty()||linked!=!d.learning_plan_sha256.empty()||
       linked!=!d.learning_base_sha256.empty())return false;
    return !linked||(digest(d.learning_plan_sha256)&&digest(d.learning_base_sha256));
  }
  static bool matches(const Definition& d,const EvaluationContext& c)noexcept {
    return c.engine_id==d.engine_id&&c.source&&c.source_epoch&&digest(c.source_state_sha256)&&
      c.model_sha256==d.model_sha256&&c.behavior_id==d.behavior_id&&
      c.metric_schema==d.metric_schema&&c.clock_domain==d.clock_domain&&c.tick_ns==d.tick_ns&&
      c.seed==d.seed&&c.input_schedule_sha256==d.input_schedule_sha256&&
      c.start_tick<=UINT64_MAX-d.optimization.horizon;
  }
  static bool same_context(const EvaluationContext& a,const EvaluationContext& b)noexcept {
    return a.engine_id==b.engine_id&&a.source==b.source&&a.source_epoch==b.source_epoch&&
      a.source_revision==b.source_revision&&a.start_tick==b.start_tick&&a.source_state_sha256==b.source_state_sha256&&
      a.model_sha256==b.model_sha256&&a.behavior_id==b.behavior_id&&a.metric_schema==b.metric_schema&&
      a.clock_domain==b.clock_domain&&a.tick_ns==b.tick_ns&&a.seed==b.seed&&a.input_schedule_sha256==b.input_schedule_sha256;
  }
  static bool valid_forecast(const Definition& d,const EvaluationContext& c,const engine::Forecast& f)noexcept {
    if(!f.valid||f.source_revision!=c.source_revision||f.source_state_sha256!=c.source_state_sha256)return false;
    for(const auto* s:{&f.baseline,&f.trial})
      if(s->id!=c.engine_id||s->source_epoch!=c.source_epoch||s->revision!=c.source_revision||
         s->tick!=c.start_tick+d.optimization.horizon||s->participants>1000000000||s->connections>1000000000||
         s->energy< -1000000000||s->energy>1000000000||s->learning_proxy< -1000000000||s->learning_proxy>1000000000||
         s->state_sha256.back()!='\0'||!digest(s->state_sha256.data()))return false;
    return true;
  }
  static std::string receipt_hash(const Definition& definition,const EvaluationContext& c,const TrialReceipt& t){
    std::ostringstream out;out<<"CEVTRIAL/1\n"<<definition.sha256<<'\n'<<static_cast<unsigned>(t.state)<<' '
      <<static_cast<unsigned>(t.status)<<'\n'<<c.engine_id<<' '<<c.source<<' '<<c.source_epoch<<' '
      <<c.source_revision<<' '<<c.start_tick<<' '<<c.seed<<'\n'<<c.model_sha256<<'\n'<<c.behavior_id<<'\n'
      <<c.source_state_sha256<<'\n'<<c.input_schedule_sha256<<'\n'<<c.metric_schema<<' '<<c.clock_domain<<' '<<c.tick_ns<<'\n'
      <<t.candidate.id<<' '<<t.candidate.policy_token<<' '<<t.candidate.cost<<' '<<t.candidate.risk<<' '<<t.candidate.uncertainty<<'\n';
    for(const auto* s:{&t.forecast.baseline,&t.forecast.trial})out<<s->tick<<' '<<s->participants<<' '<<s->connections<<' '
      <<s->energy<<' '<<s->learning_proxy<<' '<<s->state_sha256.data()<<'\n';
    return christeye::living::plans::sha256_hex(out.str());
  }
  Code start(std::string operation_id,const Definition& definition,std::string checkpoint)noexcept {
    if(persistence_fault_)return code_=Code::persistence_failed;
    if(loaded_)return code_=Code::invalid_state;
    if(!identity(operation_id)||!valid_definition(definition)||!store_.accepts_definition(definition))return code_=Code::invalid_definition;
    EvaluationContext context{};auto code=evaluator_.context(definition,context);
    if(code!=Code::ok)return code_=code;
    if(!matches(definition,context)||checkpoint.empty()||checkpoint.size()>kMaxCheckpointBytes||
       christeye::living::plans::sha256_hex(checkpoint)!=context.source_state_sha256)return code_=Code::identity_mismatch;
    Record previous{};bool exists=false;
    if(!store_.load(operation_id,previous,exists))return code_=Code::persistence_failed;
    if(exists)return code_=Code::invalid_state;
    record_={};record_.operation_id=std::move(operation_id);record_.definition=definition;
    record_.context=std::move(context);record_.source_checkpoint=std::move(checkpoint);
    if(!save())return code_;
    loaded_=true;return code_=Code::ok;
  }
  Code recover(std::string_view operation_id)noexcept {
    if(persistence_fault_)return code_=Code::persistence_failed;
    if(loaded_)return code_=Code::invalid_state;
    Record next{};bool exists=false;
    if(!store_.load(operation_id,next,exists))return code_=Code::persistence_failed;
    if(!exists||next.operation_id!=operation_id||!valid_definition(next.definition)||
       !store_.accepts_definition(next.definition)||!matches(next.definition,next.context)||
       next.next_candidate>next.definition.candidate_count||next.source_checkpoint.size()>kMaxCheckpointBytes||
       christeye::living::plans::sha256_hex(next.source_checkpoint)!=next.context.source_state_sha256)return code_=Code::corrupt;
    EvaluationContext actual{};const auto status=evaluator_.context(next.definition,actual);
    if(status!=Code::ok||!same_context(actual,next.context))return code_=Code::identity_mismatch;
    const auto state=static_cast<unsigned>(next.state);
    if(!next.revision||state<1||state>5||
       (next.state==RunState::prepared&&next.next_candidate)||
       (next.state!=RunState::completed&&next.next_candidate==next.definition.candidate_count)||
       (next.state==RunState::failed&&(next.failure==Code::ok||next.failure==Code::cancelled))||
       (next.state!=RunState::failed&&next.failure!=Code::ok))return code_=Code::corrupt;
    for(std::size_t i=0;i<next.next_candidate;++i){
      const auto& t=next.trials[i];
      if(t.state!=TrialState::completed||t.status!=Code::ok||!same_candidate(t.candidate,next.definition.candidates[i].ranking)||
         !valid_forecast(next.definition,next.context,t.forecast)||t.receipt_sha256!=receipt_hash(next.definition,next.context,t)||
         (i&&!same_snapshot(next.trials[0].forecast.baseline,t.forecast.baseline)))return code_=Code::corrupt;
    }
    for(std::size_t i=next.next_candidate;i<next.definition.candidate_count;++i){
      const auto& t=next.trials[i];
      if(next.state==RunState::failed&&i==next.next_candidate){
        if(t.state!=TrialState::failed||t.status!=next.failure||t.forecast.valid||
           !same_candidate(t.candidate,next.definition.candidates[i].ranking)||
           t.receipt_sha256!=receipt_hash(next.definition,next.context,t))return code_=Code::corrupt;
      }else if(t.state!=TrialState::pending||t.status!=Code::ok||t.candidate.id||t.forecast.valid||!t.receipt_sha256.empty())
        return code_=Code::corrupt;
    }
    if(next.state==RunState::completed&&next.next_candidate!=next.definition.candidate_count)return code_=Code::corrupt;
    if(next.state==RunState::completed){
      const auto expected=selection_for(next);const auto& actual_selection=next.selection;
      if(expected.approved!=actual_selection.approved||expected.score!=actual_selection.score||
         !same_candidate(expected.candidate,actual_selection.candidate)||
         !same_snapshot(expected.forecast.baseline,actual_selection.forecast.baseline)||
         !same_snapshot(expected.forecast.trial,actual_selection.forecast.trial)||
         expected.forecast.source_state_sha256!=actual_selection.forecast.source_state_sha256||
         expected.forecast.source_revision!=actual_selection.forecast.source_revision||
         expected.forecast.valid!=actual_selection.forecast.valid||
         actual_selection.commit_id!=next.outcome.change_id)return code_=Code::corrupt;
    }else if(next.selection.approved||next.selection.commit_id||next.outcome.change_id)return code_=Code::corrupt;
    const auto& o=next.outcome;
    if(static_cast<unsigned>(o.decision)>2||
       (o.change_id&&(!next.selection.approved||o.applied_tick!=next.context.start_tick))||
       (!o.change_id&&(o.applied_tick||o.decision!=OutcomeDecision::pending)))return code_=Code::corrupt;
    if(o.decision!=OutcomeDecision::pending){
      if(!next.definition.outcome_ticks||o.observed.id!=next.context.engine_id||
         o.observed.source_epoch!=next.context.source_epoch||o.observed.revision<next.context.source_revision||
         o.observed.tick!=o.applied_tick+next.definition.outcome_ticks||
         o.observed.state_sha256.back()!='\0'||!digest(o.observed.state_sha256.data()))return code_=Code::corrupt;
      auto decision=OutcomeDecision::retain;
      for(std::size_t i=0;i<next.definition.optimization.goal_count;++i){
        const auto& goal=next.definition.optimization.objectives[i];const auto value=metric(o.observed,goal.metric);
        if(value< -1000000000||value>1000000000||o.residuals[i]!=value-metric(next.selection.forecast.trial,goal.metric))return code_=Code::corrupt;
        if(goal.forbid_regression&&distance(value,goal.ideal)>
          distance(metric(next.selection.forecast.baseline,goal.metric),goal.ideal)+next.definition.outcome_tolerance)
          decision=OutcomeDecision::revert;
      }
      if(decision!=o.decision)return code_=Code::corrupt;
      for(std::size_t i=next.definition.optimization.goal_count;i<o.residuals.size();++i)
        if(o.residuals[i])return code_=Code::corrupt;
    }else if(o.observed.id||std::any_of(o.residuals.begin(),o.residuals.end(),[](auto n){return n!=0;}))return code_=Code::corrupt;
    record_=std::move(next);loaded_=true;return code_=Code::ok;
  }
  Code step(const ICancellation* cancel=nullptr)noexcept {
    if(persistence_fault_)return code_=Code::persistence_failed;
    if(!busy())return code_=Code::invalid_state;
    if(cancel&&cancel->cancelled())return pause();
    if(record_.state==RunState::prepared){record_.state=RunState::running;if(!save())return code_;}
    const auto i=record_.next_candidate;
    if(i>=record_.definition.candidate_count)return code_=Code::corrupt;
    engine::Forecast forecast{};const auto result=evaluator_.evaluate(record_.definition,record_.context,i,forecast,cancel);
    if(result==Code::cancelled||(cancel&&cancel->cancelled()))return pause();
    if(result!=Code::ok||!valid_forecast(record_.definition,record_.context,forecast)||
       (i&&!same_snapshot(record_.trials[0].forecast.baseline,forecast.baseline))){
      record_.state=RunState::failed;record_.failure=result==Code::ok?Code::identity_mismatch:result;
      auto& trial=record_.trials[i];trial={};trial.state=TrialState::failed;trial.status=record_.failure;
      trial.candidate=record_.definition.candidates[i].ranking;
      trial.receipt_sha256=receipt_hash(record_.definition,record_.context,trial);
      if(!save())return code_;
      return code_=record_.failure;
    }
    auto& trial=record_.trials[i];trial={};trial.state=TrialState::completed;trial.candidate=record_.definition.candidates[i].ranking;
    trial.forecast=std::move(forecast);trial.receipt_sha256=receipt_hash(record_.definition,record_.context,trial);++record_.next_candidate;
    if(record_.next_candidate==record_.definition.candidate_count){record_.selection=selection_for(record_);record_.state=RunState::completed;}
    if(!save())return code_;
    return code_=Code::ok;
  }
  Code pause()noexcept {
    if(persistence_fault_)return code_=Code::persistence_failed;
    if(!busy())return code_=Code::invalid_state;
    record_.state=RunState::paused;
    if(!save())return code_;
    return code_=Code::cancelled;
  }
  Code resume()noexcept {
    if(persistence_fault_)return code_=Code::persistence_failed;
    if(!loaded_||record_.state!=RunState::paused)return code_=Code::invalid_state;
    record_.state=RunState::running;
    if(!save())return code_;
    return code_=Code::ok;
  }
  Code mark_applied(std::uint64_t change_id,std::uint64_t tick)noexcept {
    if(persistence_fault_)return code_=Code::persistence_failed;
    if(!loaded_||record_.state!=RunState::completed||!record_.selection.approved||!change_id||
       tick!=record_.context.start_tick)return code_=Code::invalid_state;
    if(record_.outcome.change_id)return code_=(record_.outcome.change_id==change_id?Code::ok:Code::identity_mismatch);
    record_.outcome.change_id=change_id;record_.outcome.applied_tick=tick;record_.selection.commit_id=change_id;
    if(!save())return code_;
    return code_=Code::ok;
  }
  Code observe(const engine::Snapshot& observed)noexcept {
    if(persistence_fault_)return code_=Code::persistence_failed;
    if(!loaded_||!record_.outcome.change_id||!record_.definition.outcome_ticks||
       observed.id!=record_.context.engine_id||observed.source_epoch!=record_.context.source_epoch||
       observed.tick!=record_.outcome.applied_tick+record_.definition.outcome_ticks||
       observed.revision<record_.context.source_revision||observed.state_sha256.back()!='\0'||
       !digest(observed.state_sha256.data())||observed.participants>1000000000||observed.connections>1000000000||
       observed.energy< -1000000000||observed.energy>1000000000||
       observed.learning_proxy< -1000000000||observed.learning_proxy>1000000000)return code_=Code::identity_mismatch;
    if(record_.outcome.decision!=OutcomeDecision::pending)
      return code_=(same_snapshot(record_.outcome.observed,observed)?Code::ok:Code::identity_mismatch);
    auto outcome=record_.outcome;outcome.observed=observed;outcome.decision=OutcomeDecision::retain;
    for(std::size_t i=0;i<record_.definition.optimization.goal_count;++i){
      const auto& goal=record_.definition.optimization.objectives[i];const auto actual=metric(observed,goal.metric);
      if(actual< -1000000000||actual>1000000000)return code_=Code::identity_mismatch;
      outcome.residuals[i]=actual-metric(record_.selection.forecast.trial,goal.metric);
      if(goal.forbid_regression&&distance(actual,goal.ideal)>
         distance(metric(record_.selection.forecast.baseline,goal.metric),goal.ideal)+record_.definition.outcome_tolerance)
        outcome.decision=OutcomeDecision::revert;
    }
    record_.outcome=std::move(outcome);
    if(!save())return code_;
    return code_=Code::ok;
  }
};
} // namespace anaiero::experiments
