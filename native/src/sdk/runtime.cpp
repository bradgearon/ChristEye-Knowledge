#include "anaiero/sdk/runtime.hpp"
#include "anaiero/sdk/evidence.hpp"
#include "anaiero/sdk/migration_receipts.hpp"
#include "anaiero/sdk/session_record_codec.hpp"
#include "validation.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <optional>

namespace anaiero::sdk {
namespace {
constexpr std::string_view engine_kind="sdk.engine",metric_kind="sdk.metric";
std::string metric_prefix(const EngineIdentity& identity) {
  return sha256(pack({identity.source_uid,std::to_string(identity.epoch)}))+"/";
}
Bytes engine_record(const EngineIdentity& identity,std::string state,std::string transition) {
  return encode_session_engine_record({identity,std::move(state),std::move(transition)});
}
Result read_engine_record(std::string_view bytes,EngineIdentity& identity,Digest& state,Digest& transition) {
  SessionEngineRecord record;
  if(auto parsed=decode_session_engine_record(bytes,record);!parsed)return parsed;
  identity=std::move(record.identity);
  state=std::move(record.checkpoint_sha256);
  transition=std::move(record.transition_sha256);
  return {};
}
Bytes metric_record(const PreparedTransition& transition,const Observation& observation) {
  return encode_session_metric_record({transition.identity,observation,transition.target.revision});
}
Result read_metric(const evidence::StoredRecord& record,MetricView& value) {
  SessionMetricRecord parsed;
  if(auto decoded=decode_session_metric_record(record.record.payload,parsed);!decoded)return decoded;
  value.identity=std::move(parsed.identity);value.observation=std::move(parsed.observation);
  value.revision=parsed.revision;value.generation=record.generation;return {};
}
}
struct SessionService::Impl {
  struct Entry {
    std::unique_ptr<IEngine> engine;
    std::uint64_t record_revision=0,generation=0;
    Digest record_digest;
    EngineState committed_state;
    EngineIdentity identity;
  };
  struct Pending {std::size_t entry=0;PreparedTransition transition;evidence::Record record;std::uint64_t ticket=0;};
  EngineModuleCatalog& catalog;evidence::ICommitStore& store;ProjectionService& projection;
  ICommitPolicy* policy;evidence::ICommitQueue* queue=nullptr;std::size_t capacity;
  IObserverSink* observer=nullptr;bool in_call=false;
  struct Guard {bool& flag;explicit Guard(bool& value):flag(value){flag=true;}~Guard(){flag=false;}};
  std::vector<Entry> entries;std::optional<Pending> pending;
  bool degraded=false;std::string error;
  Impl(EngineModuleCatalog& c,evidence::ICommitStore& s,ProjectionService& p,ICommitPolicy* policy_value,std::size_t cap):
    catalog(c),store(s),projection(p),policy(policy_value),capacity(cap){}
  std::size_t locate(std::string_view id)const {
    for(std::size_t i=0;i<entries.size();++i)if(entries[i].identity.instance_id==id)return i;
    return entries.size();
  }
  Result admitted()const {
    if(pending)return {Code::busy,"a canonical commit is pending"};
    if(degraded)return {Code::busy,"recovery is required before another operation: "+error};
    return {};
  }
  void notify(ObserverEventKind kind,const PreparedTransition& transition,const Result& result,std::uint64_t generation)noexcept {
    if(!observer)return;
    try{(void)observer->enqueue({kind,transition.identity,transition.operation_id,transition.target.revision,
      transition.target.tick,generation,transition.target.phase,result.code});}catch(...){}
  }
  Result canonical_state(Entry& entry) {
    if(entry.engine->state()==entry.committed_state&&entry.engine->identity()==entry.identity)return {};
    try {
      if(entry.engine->identity()!=entry.identity) {
        std::unique_ptr<IEngine> replacement;
        auto restored=catalog.create(entry.identity,entry.committed_state.configuration,&entry.committed_state,replacement);
        if(restored)entry.engine=std::move(replacement);
        else {degraded=true;error="module changed identity outside the owner";}
      }else {
        auto restored=entry.engine->restore(entry.committed_state);
        if(!restored||entry.engine->state()!=entry.committed_state){degraded=true;error="module changed committed state outside the owner";}
      }
    }catch(...){degraded=true;error="module failed to restore canonical state";}
    return {Code::corrupt,"visible module state was changed outside canonical commit"};
  }
  Result duplicate(std::string_view operation,std::string_view request,bool& found) {
    evidence::CommitReceipt receipt;auto result=store.lookup_operation(operation,receipt);
    if(result.code==Code::not_found){found=false;return {};}
    if(!result)return result;
    found=true;
    if(receipt.request_digest!=request)return {Code::conflict,"operation ID reused with changed request bytes"};
    return {};
  }
  Result batch_for(const PreparedTransition& transition,const Entry& entry,evidence::CommitBatch& batch,
      evidence::Record& latest) {
    batch.operation_id=transition.operation_id;batch.request_digest=transition.request_sha256;
    if(entry.record_revision&&transition.observations.empty()&&
        (transition.target.phase==Phase::paused||transition.target.phase==Phase::cancelled)&&
        transition.target.tick==entry.committed_state.tick&&
        transition.target.checkpoint==entry.committed_state.checkpoint&&
        transition.target.configuration==entry.committed_state.configuration)
      batch.commit_class=evidence::CommitClass::maintenance;
    const auto state=encode_checkpoint(transition.identity,transition.target),prepared=encode_transition(transition);
    if(state.empty()||prepared.empty())return {Code::capacity,"canonical transition encoding bound"};
    const auto state_sha=sha256(state),transition_sha=sha256(prepared);
    batch.blobs.push_back({state_sha,state,{}});
    batch.blobs.push_back({transition_sha,prepared,{}});
    latest={std::string(engine_kind),transition.identity.instance_id,
      engine_record(transition.identity,state_sha,transition_sha),{state_sha,transition_sha},false};
    batch.records.push_back(latest);
    batch.guards.push_back({std::string(engine_kind),transition.identity.instance_id,
      entry.record_revision,entry.record_digest});
    const auto prefix=metric_prefix(transition.identity);
    for(const auto& observation:transition.observations)
      batch.records.push_back({std::string(metric_kind),prefix+observation.metric,
        metric_record(transition,observation),{},false});
    if(policy)return policy->enrich(batch,transition);
    return {};
  }
  Result finish(Pending work,const Result& committed,const evidence::CommitReceipt& receipt) {
    if(!committed.committed()) {
      if(!store.info().writable){degraded=true;error="canonical storage stopped admission: "+committed.message;}
      notify(ObserverEventKind::failed,work.transition,committed,0);return committed;
    }
    auto& entry=entries[work.entry];
    if(receipt.operation_id!=work.transition.operation_id||receipt.request_digest!=work.transition.request_sha256) {
      degraded=true;error="commit completion correlation mismatch";return {Code::committed_degraded,error};
    }
    try {
      auto installed=entry.engine->install(work.transition.target);
      if(!installed||entry.engine->state()!=work.transition.target||entry.engine->identity()!=entry.identity) {
        // Publication already committed. Reconstruct directly from the retained
        // canonical state instead of pretending the operation did not happen.
        std::unique_ptr<IEngine> restored;
        auto rebuilt=catalog.create(work.transition.identity,work.transition.target.configuration,
          &work.transition.target,restored);
        if(!rebuilt){degraded=true;error="committed engine requires recovery: "+rebuilt.message;return {Code::committed_degraded,error};}
        entry.engine=std::move(restored);
      }
    }catch(const std::exception& problem) {
      degraded=true;error=std::string("committed engine install threw: ")+problem.what();
      return {Code::committed_degraded,error};
    }catch(...){degraded=true;error="committed engine install threw";return {Code::committed_degraded,error};}
    entry.record_revision+=1;entry.record_digest=evidence::record_digest(work.record);entry.generation=receipt.generation;
    entry.committed_state=work.transition.target;
    auto projected=projection.apply(work.transition,receipt.generation);
    if(!projected){degraded=true;error="committed projection requires recovery: "+projected.message;return {Code::committed_degraded,error};}
    notify(ObserverEventKind::committed,work.transition,committed,receipt.generation);
    if(committed.code==Code::committed_degraded){degraded=true;error=committed.message;return committed;}
    return {};
  }
  Result submit(std::size_t index,PreparedTransition transition,bool asynchronous,const MigrationPlan* migration=nullptr) {
    auto projected=projection.can_apply(transition);if(!projected)return projected;
    evidence::CommitBatch batch;evidence::Record latest;
    auto built=batch_for(transition,entries[index],batch,latest);if(!built)return built;
    if(migration) {
      const auto prior=checkpoint_digest(entries[index].identity,entries[index].committed_state);
      const auto next=checkpoint_digest(transition.identity,transition.target);
      const auto encoded=encode_migration_plan(*migration);
      if(encoded.empty()||prior!=migration->source_checkpoint_sha256||
         next!=migration->target_checkpoint_sha256)return {Code::conflict,"migration state or review witness changed"};
      const auto receipt=MigrationReceipt{*migration,sha256(encode_transition(transition))};
      if(auto staged=evidence::stage_migration_receipt(batch,receipt);!staged)return staged;
    }
    Pending work{index,std::move(transition),std::move(latest),0};
    if(asynchronous&&queue) {
      auto accepted=queue->submit(std::move(batch),work.ticket);if(!accepted)return accepted;
      pending=std::move(work);return {};
    }
    evidence::CommitReceipt receipt;auto committed=store.commit(batch,receipt);
    return finish(std::move(work),committed,receipt);
  }
  Result prepare(std::string_view id,const StepRequest& request,bool change,bool asynchronous) {
    auto admission=admitted();if(!admission)return admission;
    const auto index=locate(id);if(index==entries.size())return {Code::not_found,"unknown active engine"};
    auto& engine=*entries[index].engine;
    auto canonical=canonical_state(entries[index]);if(!canonical)return canonical;
    if(!valid_token(request.operation_id)||!valid_token(request.input.schema))return {Code::invalid,"request identity"};
    bool previous=false;auto prior=duplicate(request.operation_id,request_digest(engine.identity(),request),previous);
    if(!prior||previous)return prior;
    if(request.expected_revision!=engine.state().revision)return {Code::conflict,"engine revision changed"};
    if(engine.state().phase==Phase::completed)return {Code::completed,"finite engine has completed"};
    if(engine.state().phase==Phase::cancelled||engine.state().phase==Phase::faulted||(engine.state().phase==Phase::paused&&!change))
      return {Code::conflict,"engine phase does not admit work"};
    const auto* descriptor=catalog.find(engine.identity().type_id);
    if(!descriptor||request.input.payload.size()>descriptor->limits.max_payload_bytes||
        !detail::has_schema(change?descriptor->change_schemas:descriptor->input_schemas,request.input.schema))
      return {Code::unsupported,"input schema or bound is not registered"};
    const auto before=engine.state();PreparedTransition transition;
    try {
      Result result;
      if(change) {
        auto* capability=engine.changes();if(!capability)return {Code::unsupported,"engine has no change implementation"};
        result=capability->prepare_change(request,transition);
      }else result=engine.prepare_step(request,transition);
      if(engine.state()!=before||engine.identity()!=entries[index].identity) {
        if(engine.identity()!=entries[index].identity)return canonical_state(entries[index]);
        auto restored=engine.restore(before);
        if(!restored||engine.state()!=before){degraded=true;error="prepare mutated committed state; recovery required";}
        return {Code::corrupt,"prepare mutated committed state"};
      }
      if(!result)return result;
      auto valid=detail::validate_transition(entries[index].identity,before,request,transition,descriptor,change);if(!valid)return valid;
      return submit(index,std::move(transition),asynchronous);
    }catch(const std::exception& problem){return {Code::io_error,std::string("prepare failed: ")+problem.what()};}
    catch(...){return {Code::io_error,"prepare failed"};}
  }
  Result prepare_migration(std::string_view id,const MigrationRequest& request,
                           PreparedTransition& output,MigrationPlan& plan) {
    if(auto admission=admitted();!admission)return admission;
    const auto index=locate(id);
    if(index==entries.size())return {Code::not_found,"unknown active engine"};
    auto& entry=entries[index];auto& engine=*entry.engine;
    if(auto canonical=canonical_state(entry);!canonical)return canonical;
    const auto encoded=encode_migration_request(request);
    if(encoded.empty())return {Code::invalid,"invalid migration request"};
    const auto before=entry.committed_state;
    if(request.expected_revision!=before.revision)return {Code::conflict,"migration review is stale"};
    if(before.phase!=Phase::paused)return {Code::conflict,"pause engine before a state migration"};
    if(before.checkpoint.schema!=request.source_schema)return {Code::conflict,"source checkpoint schema changed"};
    const auto* descriptor=catalog.find(entry.identity.type_id);
    if(!descriptor||!detail::has_schema(descriptor->checkpoint_schemas,request.target_schema))
      return {Code::unsupported,"target checkpoint schema is not registered"};
    if(request.parameters.payload.size()>descriptor->limits.max_payload_bytes)
      return {Code::capacity,"migration parameters exceed module bound"};
    auto* capability=engine.migrations();
    if(!capability||capability->implementation_sha256()!=request.implementation_sha256)
      return {Code::unsupported,"pinned migration implementation unavailable"};
    const StepRequest pseudo{request.operation_id,request.expected_revision,
      {"anaiero.engine-migration/1",encoded}};
    PreparedTransition transition;
    try {
      auto prepared=capability->prepare_migration(request,transition);
      if(engine.state()!=before||engine.identity()!=entry.identity) {
        auto restored=canonical_state(entry);
        if(!restored)return restored;
        return {Code::corrupt,"migration preparation mutated the committed engine"};
      }
      if(!prepared)return prepared;
      if(auto valid=detail::validate_transition(entry.identity,before,pseudo,transition,descriptor,true);!valid)
        return valid;
      if(transition.target.tick!=before.tick||transition.target.phase!=Phase::paused||
         transition.target.configuration!=before.configuration||
         transition.target.checkpoint.schema!=request.target_schema||
         !transition.observations.empty()||!transition.receipts.empty())
        return {Code::corrupt,"migration changed time, configuration, phase or emitted unreviewed effects"};
      plan={entry.identity,request,checkpoint_digest(entry.identity,before),
        checkpoint_digest(entry.identity,transition.target)};
      if(encode_migration_plan(plan).empty())return {Code::corrupt,"migration review witness invalid"};
      output=std::move(transition);return {};
    }catch(const std::exception& e){return {Code::io_error,std::string("migration preparation failed: ")+e.what()};}
    catch(...){return {Code::io_error,"migration preparation failed"};}
  }
};
SessionService::SessionService(EngineModuleCatalog& catalog,evidence::ICommitStore& store,ProjectionService& projection,
    ICommitPolicy* policy,std::size_t capacity):impl_(std::make_unique<Impl>(catalog,store,projection,policy,capacity)){}
SessionService::~SessionService()=default;
void SessionService::set_commit_queue(evidence::ICommitQueue* queue)noexcept {
  if(!impl_->pending&&!impl_->in_call)impl_->queue=queue;
}
void SessionService::set_observer_sink(IObserverSink* observer)noexcept {if(!impl_->in_call)impl_->observer=observer;}
Result SessionService::add_engine(const EngineIdentity& identity,const Envelope& configuration,const EngineState* initial) {
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);
  auto admission=impl_->admitted();if(!admission)return admission;
  if(impl_->locate(identity.instance_id)!=impl_->entries.size())return {Code::conflict,"instance already active"};
  if(!impl_->capacity||impl_->capacity>256||impl_->entries.size()==impl_->capacity)return {Code::capacity,"active engine capacity"};
  if(impl_->policy){auto allowed=impl_->policy->restore_admission(identity);if(!allowed)return allowed;}
  evidence::StoredRecord existing;auto read=impl_->store.latest(engine_kind,identity.instance_id,existing);
  if(read&& !existing.record.tombstone) {
    EngineIdentity saved_identity;Digest state_sha,transition_sha;
    auto parsed=read_engine_record(existing.record.payload,saved_identity,state_sha,transition_sha);if(!parsed)return parsed;
    if(saved_identity!=identity)return {Code::conflict,"instance has a different durable identity"};
    Bytes bytes;auto fetched=impl_->store.get_blob(state_sha,bytes);if(!fetched)return fetched;
    if(sha256(bytes)!=state_sha)return {Code::corrupt,"retained state digest mismatch"};
    EngineState state;EngineIdentity checkpoint_identity;
    auto decoded=decode_checkpoint(bytes,checkpoint_identity,state);if(!decoded)return decoded;
    if(checkpoint_identity!=identity)return {Code::corrupt,"retained checkpoint source identity mismatch"};
    if(state.configuration!=configuration||(initial&&*initial!=state))return {Code::conflict,"retained configuration/state mismatch"};
    std::unique_ptr<IEngine> engine;auto created=impl_->catalog.create(identity,state.configuration,&state,engine);if(!created)return created;
    impl_->entries.push_back({std::move(engine),existing.revision,existing.generation,existing.digest,state,identity});return {};
  }
  if(read&&existing.record.tombstone)return {Code::expired,"instance identity was retired"};
  if(!read&&read.code!=Code::not_found)return read;
  std::unique_ptr<IEngine> engine;auto created=impl_->catalog.create(identity,configuration,initial,engine);if(!created)return created;
  PreparedTransition transition;transition.identity=identity;transition.target=engine->state();
  transition.expected_revision=transition.target.revision;transition.expected_state_sha256=state_digest(transition.target);
  transition.operation_id="register-"+sha256(encode_identity(identity));
  transition.request_sha256=sha256(pack({"register/1",encode_identity(identity),encode_state(transition.target)}));
  Impl::Entry entry{std::move(engine),0,0,{},transition.target,identity};evidence::CommitBatch batch;evidence::Record record;
  auto built=impl_->batch_for(transition,entry,batch,record);if(!built)return built;
  evidence::CommitReceipt receipt;auto committed=impl_->store.commit(batch,receipt);if(!committed.committed())return committed;
  entry.record_revision=1;entry.record_digest=evidence::record_digest(record);entry.generation=receipt.generation;
  impl_->entries.push_back(std::move(entry));
  if(committed.code==Code::committed_degraded){impl_->degraded=true;impl_->error=committed.message;}
  return committed;
}
Result SessionService::recover() {
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);
  if(impl_->pending)return {Code::busy,"cannot recover during a pending commit"};
  const auto store_info=impl_->store.info();
  if(!store_info.index_ready||!store_info.writable){auto repaired=impl_->store.repair_index();if(!repaired)return repaired;}
  std::vector<Impl::Entry> next;std::string after;bool more=true;
  std::vector<EngineIdentity> active_sources;
  Result active_catalog{Code::unsupported,"no lifecycle catalog"};
  if(impl_->policy)active_catalog=impl_->policy->recovery_sources(active_sources);
  if(!active_catalog&&active_catalog.code!=Code::unsupported)return active_catalog;
  if(active_catalog&&active_sources.size()>impl_->capacity)return {Code::capacity,"active source catalog exceeds runtime capacity"};
  std::size_t active_offset=0;
  while(more) {
    std::vector<evidence::StoredRecord> page;
    if(active_catalog) {
      const auto end=std::min(active_offset+64,active_sources.size());
      for(;active_offset<end;++active_offset) {
        const auto& expected=active_sources[active_offset];evidence::StoredRecord record;
        auto read=impl_->store.latest(engine_kind,expected.instance_id,record);
        // A registered source can exist before its first attached engine state.
        if(read.code==Code::not_found)continue;
        if(!read)return read;
        EngineIdentity stored;Digest state_sha,transition_sha;
        if(record.record.tombstone)continue;
        auto decoded=read_engine_record(record.record.payload,stored,state_sha,transition_sha);if(!decoded)return decoded;
        if(stored!=expected)return {Code::corrupt,"active source/engine identity mismatch"};
        page.push_back(std::move(record));
      }
      more=active_offset<active_sources.size();
    }else {
      auto read=impl_->store.scan_latest(engine_kind,{},after,64,page,more);if(!read)return read;
    }
    if(more&&page.empty())return {Code::corrupt,"engine scan made no progress"};
    for(const auto& record:page) {
      after=record.record.key;if(record.record.tombstone)continue;
      EngineIdentity identity;Digest state_sha,transition_sha;
      auto parsed=read_engine_record(record.record.payload,identity,state_sha,transition_sha);if(!parsed)return parsed;
      if(impl_->policy){auto allowed=impl_->policy->restore_admission(identity);if(allowed.code==Code::expired)continue;if(!allowed)return allowed;}
      if(!impl_->capacity||impl_->capacity>256||next.size()==impl_->capacity)return {Code::capacity,"recovered active engine capacity"};
      Bytes bytes;auto fetched=impl_->store.get_blob(state_sha,bytes);if(!fetched)return fetched;
      if(sha256(bytes)!=state_sha)return {Code::corrupt,"recovered state digest mismatch"};
      EngineState state;EngineIdentity checkpoint_identity;
      auto decoded=decode_checkpoint(bytes,checkpoint_identity,state);if(!decoded)return decoded;
      if(checkpoint_identity!=identity)return {Code::corrupt,"recovered checkpoint source identity mismatch"};
      std::unique_ptr<IEngine> engine;auto created=impl_->catalog.create(identity,state.configuration,&state,engine);if(!created)return created;
      next.push_back({std::move(engine),record.revision,record.generation,record.digest,state,identity});
    }
  }
  std::vector<MetricView> metrics;
  for(const auto& entry:next) {
    const auto prefix=metric_prefix(entry.engine->identity());after.clear();more=true;
    while(more) {
      std::vector<evidence::StoredRecord> page;
      auto read=impl_->store.scan_latest(metric_kind,prefix,after,64,page,more);if(!read)return read;
      if(more&&page.empty())return {Code::corrupt,"metric scan made no progress"};
      for(const auto& record:page) {
        after=record.record.key;if(record.record.tombstone)continue;
        MetricView value;auto parsed=read_metric(record,value);if(!parsed)return parsed;
        if(value.identity!=entry.engine->identity()||value.revision>entry.engine->state().revision||
            value.observation.tick>entry.engine->state().tick)return {Code::corrupt,"metric exceeds canonical engine tip"};
        if(metrics.size()==65536)return {Code::capacity,"recovered metric bound"};
        metrics.push_back(std::move(value));
      }
    }
  }
  auto projected=impl_->projection.replace(metrics);if(!projected)return projected;
  impl_->entries=std::move(next);impl_->degraded=false;impl_->error.clear();return {};
}
Result SessionService::begin_step(std::string_view id,const StepRequest& request,bool change) {
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);
  return impl_->prepare(id,request,change,true);
}
Result SessionService::pump() {
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);
  if(!impl_->pending)return {};
  if(!impl_->queue)return {Code::corrupt,"pending commit has no completion port"};
  evidence::CommitCompletion completion;
  if(!impl_->queue->try_complete(completion))return {Code::busy,"canonical commit is pending"};
  if(completion.ticket!=impl_->pending->ticket){impl_->degraded=true;impl_->error="foreign commit completion ticket";return {Code::corrupt,impl_->error};}
  auto pending=std::move(*impl_->pending);impl_->pending.reset();
  return impl_->finish(std::move(pending),completion.result,completion.receipt);
}
Result SessionService::step(std::string_view id,const StepRequest& request,bool change) {
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);
  return impl_->prepare(id,request,change,false);
}
Result SessionService::control(std::string_view id,std::string operation,Phase phase) {
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);
  auto admission=impl_->admitted();if(!admission)return admission;
  const auto index=impl_->locate(id);if(index==impl_->entries.size())return {Code::not_found,"unknown active engine"};
  auto& engine=*impl_->entries[index].engine;
  auto canonical=impl_->canonical_state(impl_->entries[index]);if(!canonical)return canonical;
  if(!valid_token(operation))return {Code::invalid,"control operation identity"};
  // Control retry identity intentionally excludes mutable state/revision.
  const auto request=sha256(pack({"phase-control/1",encode_identity(engine.identity()),operation,std::string(phase_name(phase))}));
  bool previous=false;auto prior=impl_->duplicate(operation,request,previous);if(!prior||previous)return prior;
  const auto before=engine.state();
  const bool allowed=(phase==Phase::paused&&(before.phase==Phase::ready||before.phase==Phase::running))||
    (phase==Phase::running&&before.phase==Phase::paused)||
    (phase==Phase::cancelled&&(before.phase==Phase::ready||before.phase==Phase::running||before.phase==Phase::paused));
  if(!allowed)return {Code::conflict,"phase transition is not permitted"};
  if(before.revision==std::numeric_limits<std::uint64_t>::max())return {Code::capacity,"engine revision exhausted"};
  PreparedTransition transition;transition.identity=engine.identity();transition.operation_id=std::move(operation);
  transition.request_sha256=request;transition.expected_state_sha256=state_digest(before);transition.expected_revision=before.revision;
  transition.target=before;transition.target.phase=phase;++transition.target.revision;
  return impl_->submit(index,std::move(transition),false);
}
Result SessionService::review_migration(std::string_view id,const MigrationRequest& request,MigrationPlan& plan) {
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);PreparedTransition transition;
  return impl_->prepare_migration(id,request,transition,plan);
}
Result SessionService::apply_migration(std::string_view id,const MigrationPlan& plan) {
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);
  if(auto admission=impl_->admitted();!admission)return admission;
  if(plan.identity.instance_id!=id||encode_migration_plan(plan).empty())
    return {Code::invalid,"invalid pinned migration plan"};
  bool duplicate=false;
  auto prior=impl_->duplicate(plan.request.operation_id,
    migration_request_digest(plan.identity,plan.request),duplicate);
  if(!prior)return prior;
  if(duplicate) {
    evidence::StoredMigrationReceipt accepted;
    if(auto read=evidence::read_migration_receipt(impl_->store,plan.request.operation_id,accepted);!read)
      return read;
    if(encode_migration_plan(accepted.receipt.plan)!=encode_migration_plan(plan))
      return {Code::conflict,"operation was accepted with a different migration review"};
    return {};
  }
  PreparedTransition transition;MigrationPlan reviewed;
  if(auto checked=impl_->prepare_migration(id,plan.request,transition,reviewed);!checked)return checked;
  if(encode_migration_plan(reviewed)!=encode_migration_plan(plan))
    return {Code::conflict,"migration plan is stale or transformation changed"};
  const auto index=impl_->locate(id);
  const auto* descriptor=impl_->catalog.find(plan.identity.type_id);
  if(!descriptor||descriptor->limits.max_receipts==0)return {Code::capacity,"migration receipt capacity unavailable"};
  transition.receipts.push_back({"anaiero.engine-migration-receipt/1",
    pack({plan.request.implementation_sha256,plan.source_checkpoint_sha256,plan.target_checkpoint_sha256})});
  return impl_->submit(index,std::move(transition),false,&plan);
}
Result SessionService::remove_retired(std::string_view id) {
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);
  auto admission=impl_->admitted();if(!admission)return admission;
  const auto index=impl_->locate(id);
  evidence::StoredRecord stored;auto read_engine=impl_->store.latest(engine_kind,id,stored);if(!read_engine)return read_engine;
  if(stored.record.tombstone)return {};
  EngineIdentity identity;Digest state_sha,transition_sha;
  auto decoded=read_engine_record(stored.record.payload,identity,state_sha,transition_sha);if(!decoded)return decoded;
  if(!impl_->policy||impl_->policy->restore_admission(identity).code!=Code::expired)
    return {Code::conflict,"source must be durably retired before detaching its engine"};
  evidence::CommitBatch batch;batch.operation_id="detach-"+sha256(encode_identity(identity));
  batch.commit_class=evidence::CommitClass::maintenance;
  batch.request_digest=sha256(pack({"detach/1",encode_identity(identity)}));
  batch.guards.push_back({std::string(engine_kind),identity.instance_id,stored.revision,stored.digest});
  // Retain the small identity/digest witness in the tombstone, with no roots.
  // Scheduling can prove retirement after restart without retaining payloads.
  batch.records.push_back({std::string(engine_kind),identity.instance_id,stored.record.payload,{},true});
  const auto prefix=metric_prefix(identity);std::string after;bool more=true;
  while(more) {
    std::vector<evidence::StoredRecord> page;auto read=impl_->store.scan_latest(metric_kind,prefix,after,64,page,more);if(!read)return read;
    if(more&&page.empty())return {Code::corrupt,"retired metric scan made no progress"};
    for(const auto& record:page) {
      after=record.record.key;if(record.record.tombstone)continue;
      if(batch.records.size()==256)return {Code::capacity,"retired metric detach bound"};
      batch.guards.push_back({std::string(metric_kind),record.record.key,record.revision,record.digest});
      batch.records.push_back({std::string(metric_kind),record.record.key,{}, {},true});
    }
  }
  evidence::CommitReceipt receipt;auto committed=impl_->store.commit(batch,receipt);if(!committed.committed())return committed;
  impl_->projection.retire(identity.source_uid,identity.epoch);
  if(index!=impl_->entries.size())impl_->entries.erase(impl_->entries.begin()+static_cast<std::ptrdiff_t>(index));
  if(committed.code==Code::committed_degraded){impl_->degraded=true;impl_->error=committed.message;}
  return committed;
}
Result SessionService::scheduling_admission(std::string_view id,bool& eligible)const {
  eligible=false;
  if(impl_->in_call)return {Code::busy,"session owner is not reentrant"};
  Impl::Guard guard(impl_->in_call);
  auto admission=impl_->admitted();if(!admission)return admission;
  const auto index=impl_->locate(id);
  if(index!=impl_->entries.size()) {
    const auto& entry=impl_->entries[index];bool policy_eligible=true;
    if(impl_->policy){auto result=impl_->policy->scheduling_admission(entry.identity,policy_eligible);if(!result)return result;}
    eligible=policy_eligible&&(entry.committed_state.phase==Phase::ready||entry.committed_state.phase==Phase::running);
    return {};
  }
  if(!impl_->policy)return {Code::not_found,"scheduled engine is absent"};
  evidence::StoredRecord record;auto read=impl_->store.latest(engine_kind,id,record);if(!read)return read;
  EngineIdentity identity;Digest state_sha,transition_sha;
  auto decoded=read_engine_record(record.record.payload,identity,state_sha,transition_sha);if(!decoded)return decoded;
  auto lifecycle=impl_->policy->restore_admission(identity);
  if(lifecycle.code==Code::expired)return {};
  if(!lifecycle)return lifecycle;
  return {Code::corrupt,"non-retired scheduled engine is absent from active recovery"};
}
bool SessionService::busy()const noexcept{return impl_->pending.has_value();}
SessionView SessionService::view()const {
  SessionView out;out.busy=busy();out.degraded=impl_->degraded;out.error=impl_->error;
  for(const auto& entry:impl_->entries)out.engines.push_back({entry.identity,entry.committed_state,entry.generation});
  return out;
}
IEngine* SessionService::find(std::string_view id)noexcept {
  const auto index=impl_->locate(id);return index==impl_->entries.size()?nullptr:impl_->entries[index].engine.get();
}
}
