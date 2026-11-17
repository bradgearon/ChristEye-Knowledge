#pragma once
// One owner coordinates session decisions. All persistence/engine movement uses ports.
#include "anaiero/contracts/native_session.hpp"
#include "anaiero/services/simulation_coordinator_service.hpp"
#include "anaiero/services/async_observation_commit_service.hpp"
#include "anaiero/services/engine_optimization_service.hpp"
#include "anaiero/services/sw5_scenario_evaluator_service.hpp"
#include "anaiero/services/digital_route_policy_service.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <sstream>
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
    o.colony=cfg.id;o.animon=0;o.source=cfg.source;o.clock_domain=1;
    o.source_epoch=cfg.epoch;o.sequence=record_.evidence[i].next_sequence+p.metric_index;
    o.acquired_ns=snap.tick*1000000ULL;o.origin=runtime::Origin::simulated;
    for(unsigned k=0;k<16;++k){
      const char ch=snap.state_sha256[k];
      o.evidence_tag=(o.evidence_tag<<4)|static_cast<std::uint64_t>(ch<='9'?ch-'0':ch-'a'+10);
    }
    switch(p.metric_index){
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
    if(p.metric_index==5){
      auto& e=record_.evidence[p.engine_index];
      e.next_sequence+=5;e.published=p.snapshot;p={};
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
  engine::Status select(const engine::Descriptor& d,std::uint64_t,const engine::Inputs&,
                        engine::Inputs& selected)noexcept override{
    const auto i=index(d.id);if(i==config_.count)return engine::Status::invalid;
    selected=config_.engines[i].inputs;return engine::Status::ok;
  }
  bool start(Record record,bool exists)noexcept {
    if(started_||!config_.count)return false;
    coordinator_.set_input_selector(this);coordinator_.set_publisher(this);
    record_=std::move(record);
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
    started_=true;return true;
  }
  bool publish(const engine::Snapshot& snapshot)noexcept override {
    const auto i=index(snapshot.id);
    if(i==config_.count||snapshot.tick>UINT64_MAX/1000000ULL||snapshot.participants>INT32_MAX||
       snapshot.connections>INT32_MAX||snapshot.energy<INT32_MIN||snapshot.energy>INT32_MAX||
       snapshot.learning_proxy<INT32_MIN||snapshot.learning_proxy>INT32_MAX)return fail("snapshot_out_of_observation_range");
    if(same_snapshot(record_.evidence[i].published,snapshot))return true;
    if(record_.publication.active){
      if(record_.publication.engine_index!=i||!same_snapshot(record_.publication.snapshot,snapshot))
        return fail("publication_identity_conflict");
      return false;
    }
    if(record_.evidence[i].next_sequence>UINT64_MAX-5)return fail("source_sequence_exhausted");
    record_.publication={true,i,0,snapshot};
    if(!save())return false;
    return false; // asynchronous durable completion is required
  }
  bool busy()const noexcept override{
    if(!started_)return false;
    if(record_.publication.active||record_.change.active||record_.coordinator.cycle_active||
       record_.target_cycles>record_.coordinator.completed_cycles)return true;
    for(std::size_t i=0;i<config_.count;++i)if(!record_.evidence[i].published.id)return true;
    return false;
  }
  bool failed()const noexcept override{return !error_.empty();}
  bool quiescent()const noexcept override{
    return !record_.publication.active&&!record_.change.active&&!record_.coordinator.cycle_active;
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
      if(error_!="engine_step_failed")return false;
      error_.clear();return true;
    }
    if(!started_||failed()||busy())return false;
    if(budget_&&!budget_->admits(config_.count))return fail("session_capacity_reached");
    switch(command.operation){
      case Operation::step:
        if(!command.count||command.count>1024||record_.coordinator.completed_cycles>UINT64_MAX-command.count)return false;
        record_.target_cycles=record_.coordinator.completed_cycles+command.count;return save();
      case Operation::analyze:return analyze(command,cancel);
      case Operation::apply:{std::uint64_t id=0;return commit(record_.proposal.selection,id);}
      case Operation::revert:{const auto i=index(command.engine);
        return i<config_.count&&revert(record_.evidence[i].route.change_id);}
      case Operation::retry:return false;
    }
    return false;
  }
  bool commit(const engine::Selection& selected,std::uint64_t& id)noexcept override{
    id=0;
    const auto i=index(record_.proposal.engine_id);
    if(busy()||failed()||i==config_.count||!sw5_[i]||record_.evidence[i].route.active||
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
    if(!save()||!finish_change()||coordinator_.refresh_latest()!=engine::Status::ok)return fail("route_commit_recovery_required");
    record_.coordinator=coordinator_.progress();
    if(!save())return false;
    id=next.change_id;return true;
  }
  bool revert(std::uint64_t id)noexcept override{
    if(busy()||failed()||!id)return false;
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
  std::string state_json()const override{
    const auto state=projection_.latest();std::ostringstream out;
    out<<"{\"schema\":\"anaiero.session/1\",\"name\":\""<<config_.name
       <<"\",\"revision\":"<<record_.revision<<",\"cycles\":"<<record_.coordinator.completed_cycles
       <<",\"target_cycles\":"<<record_.target_cycles<<",\"busy\":"<<(busy()?"true":"false")
       <<",\"error\":\""<<error_<<"\",\"pending_evidence\":"<<evidence_.outstanding()
       <<",\"state_revision\":"<<state.revision<<",\"sequence_gaps\":"<<state.sequence_gaps<<",\"engines\":[";
    for(std::size_t i=0;i<config_.count;++i){
      if(i)out<<',';
      engine::Snapshot snap{};current(i,snap);
      out<<"{\"id\":"<<snap.id<<",\"kind\":\""<<(config_.engines[i].kind==engine::Kind::sw5?"sw5":"sim3")
         <<"\",\"source\":"<<config_.engines[i].source<<",\"epoch\":"<<snap.source_epoch
         <<",\"cadence\":"<<static_cast<unsigned>(config_.engines[i].cadence)<<",\"tick\":"<<snap.tick
         <<",\"revision\":"<<snap.revision<<",\"sha256\":\""<<snap.state_sha256.data()
         <<"\",\"route_active\":"<<(record_.evidence[i].route.active?"true":"false")<<'}';
    }
    out<<"],\"proposal\":{\"engine\":"<<record_.proposal.engine_id<<",\"approved\":"
       <<(record_.proposal.selection.approved?"true":"false")<<",\"candidate\":"<<record_.proposal.selection.candidate.id
       <<",\"score\":"<<record_.proposal.selection.score<<",\"commit_id\":"<<record_.proposal.selection.commit_id
       <<",\"source_sha256\":\""<<record_.proposal.selection.forecast.source_state_sha256<<"\"},\"metrics\":[";
    for(std::size_t i=0;i<state.count;++i){
      const auto& m=state.values[i];if(i)out<<',';
      out<<"{\"engine\":"<<m.colony<<",\"metric\":"<<static_cast<unsigned>(m.metric)
         <<",\"value\":"<<m.value<<",\"source\":"<<m.source<<",\"epoch\":"<<m.source_epoch
         <<",\"sequence\":"<<m.sequence<<",\"quality\":"<<static_cast<unsigned>(m.quality)<<'}';
    }
    out<<"]}\n";return out.str();
  }
};
} // namespace anaiero::session
