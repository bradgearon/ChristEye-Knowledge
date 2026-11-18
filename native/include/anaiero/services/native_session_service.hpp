#pragma once
// One owner coordinates session decisions. All persistence/engine movement uses ports.
#include "anaiero/contracts/native_session.hpp"
#include "anaiero/contracts/session_experiment.hpp"
#include "anaiero/contracts/scientific_observable.hpp"
#include "anaiero/services/simulation_coordinator_service.hpp"
#include "anaiero/services/async_observation_commit_service.hpp"
#include "anaiero/services/engine_optimization_service.hpp"
#include "anaiero/services/sw5_scenario_evaluator_service.hpp"
#include "anaiero/services/digital_route_policy_service.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <limits>
namespace anaiero::session {
class NativeSessionService final:public IDriver,public engine::ISnapshotPublisher,
  public engine::IEngineInputSelector,public engine::IEngineChangePort {
  const Configuration& config_;
  engine::SimulationCoordinatorService& coordinator_;
  IRecordStore& store_;
  runtime::AsyncObservationCommitService& evidence_;
  runtime::IObservationEpochProjection& projection_;
  std::array<engine::ISimulationEngine*,engine::kMaxEngines> engines_{};
  std::array<ISw5Port*,engine::kMaxEngines> sw5_{};
  Record record_{};
  std::string error_;
  bool started_=false,submitted_=false;
  contracts::IServiceExtension* extension_=nullptr;
  IWorkBudget* budget_=nullptr;
  IViewEncoder* encoder_=nullptr;
  bool cancel_pending_=false;
  IExperimentRuntime* experiments_=nullptr;
  IEngineCancellation* cancellation_=nullptr;
  contracts::ITraceView* trace_=nullptr;
  static EngineContract contract(const engine::Descriptor& d){
    const auto& c=d.capabilities;
    return {std::string(c.behavior_id),c.input_schema,c.metric_schema,c.checkpoint_schema,c.clock_domain,c.tick_ns,
      c.summaries,c.isolated_fork,c.evaluate_candidates,c.apply_changes,d.supports_restore};
  }
  std::size_t metric_count(std::size_t i)const noexcept{
    return config_.engines[i].kind==engine::Kind::scientific?1u:5u;
  }
  bool experiment_selected()const noexcept{
    const auto* run=experiments_?experiments_->record():nullptr;
    return run&&run->state==experiments::RunState::completed&&run->selection.approved&&
      run->definition.engine_id==record_.proposal.engine_id&&
      run->selection.candidate.id==record_.proposal.selection.candidate.id&&
      run->selection.forecast.source_state_sha256==record_.proposal.selection.forecast.source_state_sha256;
  }
  bool automatic_revert_due()const noexcept{
    if(record_.experiment_policy_paused)return false;
    const auto* run=experiments_?experiments_->record():nullptr;
    if(!run||!run->definition.automatic_revert||run->outcome.decision!=experiments::OutcomeDecision::revert)return false;
    const auto i=index(run->definition.engine_id);
    return i<config_.count&&record_.evidence[i].route.active&&record_.evidence[i].route.change_id==run->outcome.change_id;
  }
  bool reconcile_experiment_change()noexcept{
    if(!experiment_selected()||!record_.proposal.selection.commit_id)return true;
    const auto* run=experiments_->record();
    return experiments_->mark_applied(record_.proposal.selection.commit_id,run->context.start_tick)||
      fail("experiment_applied_receipt_failed");
  }
  bool finish_experiment()noexcept{
    const auto* run=experiments_?experiments_->record():nullptr;
    if(!run)return fail("experiment_record_missing");
    if(experiments_->failed())return fail("experiment_failed");
    if(run->state!=experiments::RunState::completed){record_.experiment_pending=false;return save();}
    if(run->outcome.change_id){record_.experiment_pending=false;return save();}
    record_.proposal={};
    if(run->definition.kind==engine::Kind::sw5){
      auto& proposal=record_.proposal;proposal.engine_id=run->definition.engine_id;
      proposal.horizon=run->definition.optimization.horizon;proposal.selection=run->selection;
      if(run->selection.approved){
        for(std::size_t n=0;n<run->definition.candidate_count;++n){
          const auto& c=run->definition.candidates[n];
          if(c.ranking.id!=run->selection.candidate.id)continue;
          std::uint32_t from=0,to=0;
          const auto a=std::from_chars(c.from.data(),c.from.data()+c.from.size(),from);
          const auto b=std::from_chars(c.to.data(),c.to.data()+c.to.size(),to);
          if(a.ec!=std::errc{}||b.ec!=std::errc{}||!from||!to)return fail("invalid_experiment_route");
          proposal.option={{c.ranking.id,proposal.engine_id,from,to,optimization::RouteKind::digital_signal,
            c.ranking.cost,c.ranking.risk,c.value},c.value,c.ranking.uncertainty};
        }
      }
    }
    if(run->definition.automatic_apply&&run->selection.approved){
      if(!save())return false;
      std::uint64_t id=0;if(!commit(record_.proposal.selection,id))return fail("experiment_apply_conflict");
    }
    record_.experiment_pending=false;return save();
  }
  static bool same_snapshot(const engine::Snapshot& a,const engine::Snapshot& b)noexcept{
    return a.id==b.id&&a.source_epoch==b.source_epoch&&a.tick==b.tick&&
      a.revision==b.revision&&a.state_sha256==b.state_sha256;
  }
  std::size_t index(std::uint32_t id)const noexcept{
    for(std::size_t i=0;i<config_.count;++i)if(config_.engines[i].id==id)return i;
    return config_.count;
  }
  bool fail(std::string_view error)noexcept {error_=error;return false;}
  bool save()noexcept {
    if(!store_.save(record_))return fail("session_manifest_commit_failed");
    if(extension_)extension_->on_service_event({contracts::ServiceId::runtime_dispatch,
      contracts::ServicePhase::committed,record_.revision,0,0});
    return true;
  }
  bool current(std::size_t i,engine::Snapshot& snap)const noexcept{
    return i<config_.count&&engines_[i]&&engines_[i]->latest(snap)==engine::Status::ok;
  }
  bool finish_change()noexcept {
    const auto i=record_.change.engine_index;
    if(i>=config_.count||!sw5_[i])return fail("invalid_pending_change");
    engine::Snapshot before{};
    if(!current(i,before))return fail("change_engine_unavailable");
    // The target is a sealed checkpoint stored in the same durable manifest as
    // its route state. replace verifies the checkpoint's model and structure.
    if(std::string_view(before.state_sha256.data())!=record_.change.source_sha256){
      std::string actual;cells::World world{};
      if(!sw5_[i]->world(world)||!sw5_[i]->encode(world,actual)||
         actual!=record_.change.target_checkpoint)return fail("change_source_conflict");
    }
    if(!sw5_[i]->replace(record_.change.target_checkpoint))return fail("change_checkpoint_failed");
    engine::Snapshot after{};
    if(!current(i,after))return fail("change_engine_unavailable");
    record_.coordinator.engines[i].last=after;
    return true;
  }
  runtime::Observation observation()const noexcept {
    const auto& p=record_.publication;const auto i=p.engine_index;
    const auto& snap=p.snapshot;const auto& cfg=config_.engines[i];
    runtime::Observation o{};
    const auto capabilities=engines_[i]->describe().capabilities;
    o.colony=cfg.id;o.animon=0;o.source=cfg.source;o.clock_domain=config_.sustained_storage?capabilities.clock_domain:1u;
    o.source_epoch=cfg.epoch;o.sequence=record_.evidence[i].next_sequence+p.metric_index;
    o.acquired_ns=snap.tick*(config_.sustained_storage?capabilities.tick_ns:1000000ULL);o.origin=runtime::Origin::simulated;
    for(unsigned k=0;k<16;++k){
      const char ch=snap.state_sha256[k];
      o.evidence_tag=(o.evidence_tag<<4)|static_cast<std::uint64_t>(ch<='9'?ch-'0':ch-'a'+10);
    }
    if(cfg.kind==engine::Kind::scientific){
      const auto* observable=science::observable_for_summary(capabilities.summaries[2]);
      // A scientific engine may publish only its declared supported observable.
      // Invalid semantics fail normal observation admission, never become energy.
      o.metric=observable?observable->metric:static_cast<runtime::Metric>(0);
      o.value=static_cast<std::int32_t>(snap.energy);
    }else switch(p.metric_index){
      case 0:o.metric=runtime::Metric::viability;o.value=static_cast<std::int32_t>(snap.participants);break;
      case 1:o.metric=runtime::Metric::energy;o.value=static_cast<std::int32_t>(snap.energy);break;
      case 2:o.metric=runtime::Metric::connection;o.value=static_cast<std::int32_t>(snap.connections);break;
      case 3:o.metric=runtime::Metric::digital_learning;o.value=static_cast<std::int32_t>(snap.learning_proxy);break;
      default:o.metric=runtime::Metric::digital_route_active;o.value=record_.evidence[i].route.active?1:0;break;
    }
    o.lane=p.metric_index<2?runtime::Lane::health:runtime::Lane::evidence;
    return o;
  }
  bool drain_publication()noexcept {
    auto& p=record_.publication;
    if(!p.active)return false;
    if(submitted_){
      if(!evidence_.poll()){
        if(evidence_.failed())return fail("evidence_commit_failed_restart_required");
        return false;
      }
      submitted_=false;++p.metric_index;
      if(!save())return false;
    }
    if(p.metric_index==metric_count(p.engine_index)){
      auto& e=record_.evidence[p.engine_index];
      const auto committed=p.snapshot;
      e.next_sequence+=metric_count(p.engine_index);e.published=p.snapshot;p={};
      const auto* run=experiments_?experiments_->record():nullptr;
      if(run&&!record_.experiment_policy_paused&&run->outcome.change_id&&run->definition.outcome_ticks&&run->definition.engine_id==committed.id&&
         committed.tick==run->outcome.applied_tick+run->definition.outcome_ticks&&
         run->outcome.decision==experiments::OutcomeDecision::pending&&!experiments_->observe(committed))
        return fail("experiment_outcome_commit_failed");
      return save();
    }
    if(!evidence_.submit(observation()))return fail("evidence_admission_failed");
    submitted_=true;return true;
  }
  bool analyze(const Command& command,const ICancellation* cancel)noexcept {
    const auto i=index(command.engine);
    if(i>=config_.count||!sw5_[i]||!command.from||!command.to||command.from==command.to||
       !command.count||command.count>128)return false;
    engine::Snapshot source{};cells::World initial{};
    if(!current(i,source)||!sw5_[i]->world(initial)||!source.revision)return false;
    auto& port=*sw5_[i];const auto& in=config_.engines[i].inputs;
    engine::Sw5ScenarioEvaluatorService evaluator(port.atlas(),initial,port.spatial(),
      {in.support_proxy,in.stress_proxy,in.digital_signal,in.lesson},source.id,
      config_.engines[i].source,source.source_epoch,source.revision,source.state_sha256.data());
    const std::array<engine::Candidate,3> candidates{{{1,1,2,1,1},{2,2,3,1,2},{3,3,300,100,100}}};
    const std::array<std::uint16_t,3> weights{{400,700,900}};
    std::array<optimization::ForkOption,3> choices{};
    for(std::size_t j=0;j<choices.size();++j){
      const auto& c=candidates[j];
      choices[j]={{c.id,source.id,command.from,command.to,optimization::RouteKind::digital_signal,
        c.cost,c.risk,weights[j]},weights[j],c.uncertainty};
      if(!evaluator.add(c,choices[j]))return false;
    }
    struct Cancellable final:engine::IScenarioEvaluator {
      engine::IScenarioEvaluator& inner;const ICancellation* cancel;
      Cancellable(engine::IScenarioEvaluator& value,const ICancellation* token):inner(value),cancel(token){}
      bool evaluate(const engine::OptimizationPlan& p,const engine::Candidate& c,engine::Forecast& f)noexcept override{
        return (!cancel||!cancel->cancelled())&&inner.evaluate(p,c,f)&&(!cancel||!cancel->cancelled());
      }
    } bounded(evaluator,cancel);
    engine::OptimizationPlan plan{};plan.horizon=static_cast<std::uint16_t>(command.count);
    plan.goal_count=3;plan.minimum_score=1;
    plan.objectives[0]={engine::ObjectiveMetric::connections,static_cast<std::int64_t>(source.connections)+1,6,true};
    plan.objectives[1]={engine::ObjectiveMetric::participants,source.participants,2,true};
    plan.objectives[2]={engine::ObjectiveMetric::learning_proxy,source.learning_proxy+40,1,false};
    engine::EngineOptimizationService optimizer(bounded);optimizer.set_extension(extension_);
    Proposal proposal{};proposal.engine_id=source.id;proposal.horizon=plan.horizon;
    const bool approved=optimizer.analyze(plan,candidates,proposal.selection);
    if(cancel&&cancel->cancelled())return false;
    if(approved)proposal.option=choices[proposal.selection.candidate.id-1];
    record_.proposal=std::move(proposal);
    return save(); // no improvement is an inspectable successful analysis
  }
public:
  NativeSessionService(const Configuration& config,engine::SimulationCoordinatorService& coordinator,
      IRecordStore& store,runtime::AsyncObservationCommitService& evidence,
      runtime::IObservationEpochProjection& projection,
      std::array<engine::ISimulationEngine*,engine::kMaxEngines> engines,
      std::array<ISw5Port*,engine::kMaxEngines> sw5)noexcept:
    config_(config),coordinator_(coordinator),store_(store),evidence_(evidence),projection_(projection),
    engines_(engines),sw5_(sw5){}
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  void set_budget(IWorkBudget* budget)noexcept{budget_=budget;}
  void set_view_encoder(IViewEncoder* encoder)noexcept{encoder_=encoder;}
  void set_experiments(IExperimentRuntime* experiments)noexcept{experiments_=experiments;}
  void set_cancellation(IEngineCancellation* cancellation)noexcept{cancellation_=cancellation;}
  void set_trace(contracts::ITraceView* trace)noexcept{trace_=trace;}
  engine::Status select(const engine::Descriptor& d,std::uint64_t,const engine::Inputs&,
                        engine::Inputs& selected)noexcept override{
    const auto i=index(d.id);if(i==config_.count)return engine::Status::invalid;
    selected=config_.engines[i].inputs;
    const auto* run=experiments_?experiments_->record():nullptr;
    if(run&&!record_.experiment_policy_paused&&run->definition.engine_id==d.id&&run->outcome.change_id&&
       run->outcome.decision==experiments::OutcomeDecision::pending){
      engine::Snapshot source{};
      if(!current(i,source)||source.tick<run->outcome.applied_tick)return engine::Status::invalid;
      auto offset=source.tick-run->outcome.applied_tick;
      for(std::size_t stage=0;stage<run->definition.stage_count;++stage){
        const auto& scheduled=run->definition.stages[stage];
        if(offset<scheduled.ticks){selected=scheduled.inputs;break;}
        offset-=scheduled.ticks;
      }
    }
    return engine::Status::ok;
  }
  bool start(Record record,bool exists)noexcept {
    if(started_||!config_.count)return false;
    coordinator_.set_input_selector(this);coordinator_.set_publisher(this);
    record_=std::move(record);
    cancel_pending_=record_.operation_cancelled&&(record_.coordinator.cycle_active||record_.publication.active||
      record_.change.active||record_.target_cycles>record_.coordinator.completed_cycles||record_.experiment_pending);
    for(std::size_t i=0;i<config_.count;++i){
      const auto expected=contract(engines_[i]->describe());
      if(config_.engines[i].kind==engine::Kind::scientific){
        const auto* observable=science::observable_for_summary(expected.summaries[2]);
        if(!observable||expected.input_schema!=observable->input_schema||
           expected.metric_schema!=observable->metric_schema||
           expected.summaries[0]!=engine::SummarySemantic::unsupported||
           expected.summaries[1]!=engine::SummarySemantic::unsupported||
           expected.summaries[3]!=engine::SummarySemantic::unsupported||
           expected.isolated_fork||expected.evaluate_candidates||expected.apply_changes)
          return fail("unsupported_scientific_observable");
      }
      if(exists&&record_.schema>=2&&record_.contracts[i]!=expected)return fail("engine_contract_changed");
      record_.contracts[i]=expected;
    }
    if(record_.experiment_operation_id&&(!experiments_||!experiments_->recover(record_.experiment_operation_id)))
      return fail("experiment_recovery_failed");
    if(exists){
      if(record_.configuration_sha256!=config_.digest||record_.coordinator.engine_count!=config_.count)
        return fail("session_configuration_changed");
      if(record_.change.active&&!finish_change())return false;
      if(coordinator_.resume(record_.coordinator,true)!=engine::Status::ok)
        return fail("session_engine_recovery_conflict");
    }else{
      record_.configuration_sha256=config_.digest;
      if(coordinator_.initialize_from_latest()!=engine::Status::ok)return fail("engine_initialization_failed");
      record_.coordinator=coordinator_.progress();
      if(!save())return false;
    }
    if(!reconcile_experiment_change())return false;
    started_=true;return true;
  }
  bool publish(const engine::Snapshot& snapshot)noexcept override {
    const auto i=index(snapshot.id);
    const auto tick_ns=i<config_.count?(config_.sustained_storage?engines_[i]->describe().capabilities.tick_ns:1000000ULL):0;
    if(i==config_.count||(tick_ns&&snapshot.tick>UINT64_MAX/tick_ns)||snapshot.participants>INT32_MAX||
       snapshot.connections>INT32_MAX||snapshot.energy<INT32_MIN||snapshot.energy>INT32_MAX||
       snapshot.learning_proxy<INT32_MIN||snapshot.learning_proxy>INT32_MAX)return fail("snapshot_out_of_observation_range");
    if(same_snapshot(record_.evidence[i].published,snapshot))return true;
    if(record_.publication.active){
      if(record_.publication.engine_index!=i||!same_snapshot(record_.publication.snapshot,snapshot))
        return fail("publication_identity_conflict");
      return false;
    }
    if(record_.evidence[i].next_sequence>UINT64_MAX-metric_count(i))return fail("source_sequence_exhausted");
    record_.publication={true,i,0,snapshot};
    if(!save())return false;
    return false; // asynchronous durable completion is required
  }
  bool busy()const noexcept override{
    if(!started_)return false;
    if(cancel_pending_||record_.experiment_pending||automatic_revert_due())return true;
    if(record_.publication.active||record_.change.active||record_.coordinator.cycle_active||
       record_.target_cycles>record_.coordinator.completed_cycles)return true;
    for(std::size_t i=0;i<config_.count;++i)if(!record_.evidence[i].published.id)return true;
    return false;
  }
  bool failed()const noexcept override{return !error_.empty();}
  bool quiescent()const noexcept override{
    return !record_.publication.active&&!record_.change.active&&!record_.coordinator.cycle_active&&
      (!experiments_||!experiments_->busy());
  }
  std::uint64_t revision()const noexcept override{return record_.revision;}
  const Record& record()const noexcept{return record_;}
  bool pump()noexcept override{
    if(!started_||failed())return false;
    if(record_.publication.active)return drain_publication();
    if(record_.change.active){
      const auto i=record_.change.engine_index;engine::Snapshot snap{};
      if(!current(i,snap))return fail("changed_engine_unavailable");
      if(!same_snapshot(record_.evidence[i].published,snap)){publish(snap);return !failed();}
      record_.change={};return save();
    }
    if(!record_.coordinator.cycle_active){
      if(cancel_pending_){record_.target_cycles=record_.coordinator.completed_cycles;cancel_pending_=false;return save();}
      if(record_.experiment_pending){
        if(experiments_&&experiments_->busy()){
          const bool progressed=experiments_->pump();
          if(experiments_->failed())return fail("experiment_failed");
          return progressed;
        }
        return finish_experiment();
      }
      if(automatic_revert_due())return revert(experiments_->record()->outcome.change_id);
      for(std::size_t i=0;i<config_.count;++i)if(!record_.evidence[i].published.id){
        engine::Snapshot snap{};if(!current(i,snap))return fail("initial_snapshot_unavailable");
        publish(snap);return !failed();
      }
      if(record_.target_cycles<=record_.coordinator.completed_cycles)return false;
      if(budget_&&!budget_->admits(config_.count))return fail("session_capacity_reached");
      if(coordinator_.begin_cycle()!=engine::Status::ok)return fail("cycle_start_failed");
      record_.coordinator=coordinator_.progress();
      return save(); // intent must reach disk before an engine can advance
    }
    const auto status=coordinator_.step_cycle();
    record_.coordinator=coordinator_.progress();
    if(!save())return false;
    if(status!=engine::Status::ok&&status!=engine::Status::publish_failed)return fail("engine_step_failed");
    return true;
  }
  bool execute(const Command& command,const ICancellation* cancel)noexcept override{
    if(command.operation==Operation::retry){
      // A persisted RETRY can reopen after a fresh engine has already cleared
      // the transient error. Rebind its existing continuation without a step.
      if(error_.empty()){
        if(!started_||!busy())return false;
      }else if(error_!="engine_step_failed"&&error_!="experiment_failed"&&error_!="experiment_apply_conflict")return false;
      if(error_=="experiment_failed"||error_=="experiment_apply_conflict"||
         (record_.experiment_pending&&experiments_&&experiments_->failed()))record_.experiment_pending=false;
      error_.clear();record_.operation_id=command.operation_id;return save();
    }
    if(!started_||failed()||busy())return false;
    if(budget_&&!budget_->admits(config_.count))return fail("session_capacity_reached");
    const auto previous_operation=record_.operation_id;
    cancel_pending_=false;
    record_.operation_cancelled=false;
    record_.operation_id=command.operation_id;
    switch(command.operation){
      case Operation::step:
        if(!command.count||command.count>1024||record_.coordinator.completed_cycles>UINT64_MAX-command.count){record_.operation_id=previous_operation;return false;}
        record_.target_cycles=record_.coordinator.completed_cycles+command.count;return save();
      case Operation::analyze:return analyze(command,cancel);
      case Operation::apply:{std::uint64_t id=0;return commit(record_.proposal.selection,id);}
      case Operation::revert:{const auto i=index(command.engine);
        return i<config_.count&&revert(record_.evidence[i].route.change_id);}
      case Operation::retry:return false;
      case Operation::experiment:{
        const auto* previous=experiments_?experiments_->record():nullptr;
        if(!experiments_||!command.operation_id||(!record_.experiment_policy_paused&&previous&&previous->outcome.change_id&&
           previous->definition.outcome_ticks&&previous->outcome.decision==experiments::OutcomeDecision::pending)||
           !experiments_->start(command.operation_id,command.reference)){record_.operation_id=previous_operation;return false;}
        record_.experiment_operation_id=command.operation_id;record_.experiment_pending=true;
        record_.experiment_policy_paused=false;return save();
      }
      case Operation::resume_experiment:
        if(!experiments_||!record_.experiment_operation_id||!experiments_->resume()){record_.operation_id=previous_operation;return false;}
        record_.experiment_pending=true;record_.experiment_policy_paused=false;return save();
    }
    return false;
  }
  bool commit(const engine::Selection& selected,std::uint64_t& id)noexcept override{
    id=0;
    const auto i=index(record_.proposal.engine_id);
    if(!quiescent()||failed()||i==config_.count||!sw5_[i]||record_.evidence[i].route.active||
       !selected.approved||selected.commit_id||selected.score<=0||record_.changes==UINT64_MAX||
       selected.candidate.id!=record_.proposal.selection.candidate.id||
       selected.forecast.source_state_sha256!=record_.proposal.selection.forecast.source_state_sha256)return false;
    engine::Snapshot snapshot{};cells::World before{},after{};
    if(!current(i,snapshot)||snapshot.revision!=selected.forecast.source_revision||
       snapshot.source_epoch!=selected.forecast.baseline.source_epoch||
       snapshot.id!=selected.forecast.baseline.id||
       std::string_view(snapshot.state_sha256.data())!=selected.forecast.source_state_sha256||
       !sw5_[i]->world(before))return false;
    auto& port=*sw5_[i];optimization::DigitalRoutePolicyService policy(port.atlas(),port.spatial());
    RouteState next{};next.active=true;next.selected=record_.proposal.option.route;next.change_id=record_.changes+1;
    if(!policy.apply(before,record_.proposal.option,after,next.prior_existed,next.prior_guidance))return false;
    std::string target;if(!port.encode(after,target))return false;
    record_.change={true,i,snapshot.state_sha256.data(),std::move(target)};
    record_.evidence[i].route=next;record_.changes=next.change_id;
    record_.proposal.selection.commit_id=next.change_id;
    if(experiment_selected())record_.experiment_policy_paused=false;
    const auto* run=experiment_selected()?experiments_->record():nullptr;
    if(run&&run->definition.automatic_apply&&run->definition.outcome_ticks){
      const auto cadence=config_.engines[i].cadence;
      const auto cycles=record_.coordinator.completed_cycles/cadence+run->definition.outcome_ticks;
      if(cycles>UINT64_MAX/cadence)return fail("experiment_outcome_clock_exhausted");
      record_.target_cycles=std::max(record_.target_cycles,cycles*cadence);
    }
    if(!save()||!finish_change()||coordinator_.refresh_latest()!=engine::Status::ok)return fail("route_commit_recovery_required");
    record_.coordinator=coordinator_.progress();
    if(!save())return false;
    if(!reconcile_experiment_change())return false;
    id=next.change_id;return true;
  }
  bool revert(std::uint64_t id)noexcept override{
    if(!quiescent()||failed()||!id)return false;
    for(std::size_t i=0;i<config_.count;++i){
      const auto route=record_.evidence[i].route;
      if(!route.active||route.change_id!=id||!sw5_[i])continue;
      auto& port=*sw5_[i];engine::Snapshot snapshot{};cells::World before{},after{};
      if(!current(i,snapshot)||!port.world(before))return false;
      optimization::DigitalRoutePolicyService policy(port.atlas(),port.spatial());
      if(!policy.undo(before,route.selected,route.prior_existed,route.prior_guidance,after))return false;
      std::string target;if(!port.encode(after,target))return false;
      record_.change={true,i,snapshot.state_sha256.data(),std::move(target)};
      record_.evidence[i].route.active=false;
      if(!save()||!finish_change()||coordinator_.refresh_latest()!=engine::Status::ok)return fail("route_revert_recovery_required");
      record_.coordinator=coordinator_.progress();return save();
    }
    return false;
  }
  std::uint64_t operation_id()const noexcept override{return record_.operation_id;}
  void cancel_operations()noexcept override{
    const bool draining=busy();
    const auto* run=experiments_?experiments_->record():nullptr;
    const bool armed=run&&!record_.experiment_policy_paused&&run->outcome.change_id&&run->definition.outcome_ticks&&
      run->outcome.decision==experiments::OutcomeDecision::pending;
    if(!draining&&!armed)return;
    if(experiments_&&experiments_->busy()&&!experiments_->cancel()){fail("experiment_pause_failed");return;}
    // A completed trial is not permission to apply after a cancellation. This
    // flag and cleared intent survive a crash between cancellation and drain.
    record_.experiment_pending=false;record_.operation_cancelled=record_.operation_cancelled||draining;
    record_.experiment_policy_paused=true;cancel_pending_=draining;
    (void)save();
  }
  void request_cancel()noexcept override{if(cancellation_)cancellation_->request_cancel();}
  View view()const override{
    View result{};result.name=config_.name;result.error=error_;result.revision=record_.revision;
    result.cycles=record_.coordinator.completed_cycles;result.target_cycles=record_.target_cycles;
    result.operation_id=record_.operation_id;result.busy=busy();result.pending_evidence=evidence_.outstanding();
    result.projected=projection_.latest();result.engine_count=config_.count;
    if(budget_)result.storage=budget_->storage();
    for(std::size_t i=0;i<config_.count;++i){
      auto& e=result.engines[i];current(i,e.working);const auto descriptor=engines_[i]->describe();
      e.kind=descriptor.kind;e.capabilities=descriptor.capabilities;e.source=descriptor.source;
      e.model_sha256=descriptor.model_digest;e.classification=descriptor.classification;
      e.cadence=config_.engines[i].cadence;e.published=record_.evidence[i].published;
      e.coordinator_revision=record_.coordinator.engines[i].last.revision;
      e.route_active=record_.evidence[i].route.active;
      e.publication_pending=record_.publication.active&&record_.publication.engine_index==i;
    }
    const auto& p=record_.proposal;result.proposal_engine=p.engine_id;
    result.proposal_approved=p.selection.approved;result.proposal_candidate=p.selection.candidate.id;
    result.proposal_score=p.selection.score;result.proposal_commit=p.selection.commit_id;
    result.proposal_source=p.selection.forecast.source_state_sha256;
    if(const auto* run=experiments_?experiments_->record():nullptr){
      result.experiment_id=run->operation_id;
      switch(run->state){
        case experiments::RunState::prepared:result.experiment_state="prepared";break;
        case experiments::RunState::running:result.experiment_state="running";break;
        case experiments::RunState::paused:result.experiment_state="paused";break;
        case experiments::RunState::completed:result.experiment_state="completed";break;
        case experiments::RunState::failed:result.experiment_state="failed";break;
      }
      result.completed_trials=static_cast<std::uint32_t>(run->next_candidate);
      result.total_trials=static_cast<std::uint32_t>(run->definition.candidate_count);
    }
    return result;
  }
  std::string state_json()const override{return encoder_?encoder_->encode(view()):"{}\n";}
  std::string trace_json()const override{return trace_?trace_->trace_json():"{\"events\":[]}\n";}
  ExperimentView experiment_view()const override{
    ExperimentView view{};const auto* run=experiments_?experiments_->record():nullptr;if(!run)return view;
    view.id=run->operation_id;view.definition_sha256=run->definition.sha256;view.source_sha256=run->context.source_state_sha256;
    view.model_sha256=run->context.model_sha256;view.behavior=run->context.behavior_id;
    view.engine=run->context.engine_id;view.source=run->context.source;view.epoch=run->context.source_epoch;
    view.start_tick=run->context.start_tick;view.seed=run->context.seed;view.state=run->state;view.failure=run->failure;
    view.count=run->definition.candidate_count;view.completed=run->next_candidate;view.trials=run->trials;
    view.selection=run->selection;view.outcome=run->outcome;view.policy_paused=record_.experiment_policy_paused;return view;
  }
};
} // namespace anaiero::session
