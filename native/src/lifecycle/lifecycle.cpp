#include "records.hpp"
#include <map>
#include <utility>

namespace anaiero::sdk::lifecycle {
using namespace detail;
namespace ev=evidence;
namespace {
constexpr std::string_view active_kind="lifecycle.active",source_kind="lifecycle.source",
  fence_kind="lifecycle.fence",peer_kind="lifecycle.peer",subscription_kind="lifecycle.subscription",
  pin_kind="lifecycle.pin",maintenance_kind="lifecycle.maintenance",floor_kind="lifecycle.floor",
  range_kind="lifecycle.range",object_kind="lifecycle.object";
Result failure(Code code,std::string message){return {code,std::move(message)};}
void guard(ev::CommitBatch& batch,std::string_view kind,std::string_view key,const ev::StoredRecord& previous) {
  batch.guards.push_back({std::string(kind),std::string(key),previous.revision,previous.digest});
}
void put(ev::CommitBatch& batch,std::string_view kind,std::string key,Bytes bytes,
         std::vector<Digest> references={},bool tombstone=false) {
  batch.records.push_back({std::string(kind),std::move(key),std::move(bytes),std::move(references),tombstone});
}
bool unique_digests(const std::vector<Digest>& objects) {
  std::set<Digest> seen;
  for(const auto& digest:objects)if(!valid_digest(digest)||!seen.insert(digest).second)return false;
  return true;
}
Result add_blob(ev::CommitBatch& batch,Digest digest,Bytes bytes) {
  for(const auto& previous:batch.blobs)if(previous.digest==digest)
    return previous.bytes==bytes?Result{}:failure(Code::conflict,"same digest has different staged bytes");
  batch.blobs.push_back({std::move(digest),std::move(bytes),{}});return {};
}
std::vector<Digest> source_references(const Source& source) {
  if(source.phase!=SourcePhase::retired&&!source.state_object_sha256.empty())return {source.state_object_sha256};
  return {};
}
std::vector<Digest> active_references(const Active& active) {
  std::set<Digest> refs;
  for(const auto& source:active.view.sources)if(!source.state_object_sha256.empty())refs.insert(source.state_object_sha256);
  for(const auto& metric:active.view.metrics)refs.insert(sha256(encode_source_observation(metric.source,metric.sequence,metric.observation)));
  return {refs.begin(),refs.end()};
}
bool lease_valid(std::uint64_t until,std::uint64_t now,std::uint64_t maximum) {
  return until>now&&until-now<=maximum;
}
std::string range_key(const SourceRef& source,std::uint64_t end) {
  const auto digits=number(end);return source_key(source)+":"+std::string(20-digits.size(),'0')+digits;
}
}

struct Service::Impl {
  ev::ICommitStore& store;Options options;IArchiveStore* archive;
  Impl(ev::ICommitStore& value,Options bounds,IArchiveStore* archives):store(value),options(bounds),archive(archives){}
  Result read(std::string_view kind,std::string_view key,ev::StoredRecord& out,bool& found) const {
    out={};const auto result=store.latest(kind,key,out);
    found=result.ok();if(result.code==Code::not_found)return {};return result;
  }
  Result active(Active& out,ev::StoredRecord& previous) const {
    bool found=false;const auto result=read(active_kind,"state",previous,found);if(!result)return result;
    out={};if(!found)return {};
    if(previous.record.tombstone||!decode(previous.record.payload,out,options))return bad("invalid lifecycle active checkpoint");
    return {};
  }
  template<class T> Result load(std::string_view kind,std::string_view key,T& out,ev::StoredRecord& previous) const {
    bool found=false;const auto result=read(kind,key,previous,found);if(!result)return result;
    if(!found||previous.record.tombstone)return failure(Code::not_found,"lifecycle record not found");
    out={};if(!decode(previous.record.payload,out))return bad("invalid lifecycle record");return {};
  }
  Result source(const SourceRef& ref,Source& out,ev::StoredRecord& previous) const {
    if(!valid_ref(ref))return failure(Code::invalid,"invalid source identity");
    const auto result=load(source_kind,source_key(ref),out,previous);if(!result)return result;
    return out.id==ref?Result{}:bad("source key identity mismatch");
  }
  Result start(ev::CommitBatch& batch,std::string operation,Bytes request,bool& duplicate) const {
    if(!valid_token(operation,256))return failure(Code::invalid,"invalid lifecycle operation id");
    batch.operation_id=std::move(operation);batch.request_digest=sha256(request);
    ev::CommitReceipt previous;auto result=store.lookup_operation(batch.operation_id,previous);
    duplicate=false;
    if(result.ok()){
      if(previous.request_digest!=batch.request_digest)return failure(Code::conflict,"operation id was used for another request");
      duplicate=true;return {};
    }
    return result.code==Code::not_found?Result{}:result;
  }
  Result commit(ev::CommitBatch& batch) const {ev::CommitReceipt receipt;return store.commit(batch,receipt);}
  Result save_active(ev::CommitBatch& batch,Active& active,const ev::StoredRecord& previous) const {
    if(!add(active.view.revision))return failure(Code::capacity,"active view revision exhausted");
    guard(batch,active_kind,"state",previous);
    put(batch,active_kind,"state",encode(active),active_references(active));return {};
  }
  Result time(Active& active,std::uint64_t now) const {
    if(now<active.last_unix_s)return failure(Code::expired,"clock is behind the last durable authority time");
    active.last_unix_s=now;return {};
  }
  Result available(const Active& active) const {
    return active.quarantined?failure(Code::unauthorized,"restored store is quarantined"):Result{};
  }
  Result authority(const Active& active,std::string_view principal,std::uint64_t now,
                   ev::CommitBatch* batch=nullptr,std::string scope="local",std::uint64_t lease=0) const {
    if(auto result=available(active);!result)return result;
    if(!contains(active.peers,principal))return failure(Code::unauthorized,"peer is not in the active authority directory");
    Peer peer;ev::StoredRecord stored;const auto key=sha256(principal);
    if(auto result=load(peer_kind,key,peer,stored);!result)return result;
    if(peer.principal!=principal)return bad("peer identity mismatch");
    if(peer.phase!=PeerPhase::active||peer.expires_unix_s<=now)return failure(Code::unauthorized,"peer grant is revoked or expired");
    if(batch){
      guard(*batch,peer_kind,key,stored);
      const auto deadline=lease?std::min(lease,peer.expires_unix_s):peer.expires_unix_s;
      batch->authorizations.push_back({peer.principal,std::move(scope),peer.generation,deadline});
    }
    return {};
  }
  Result cursor(const SourceRef& source,const Cursor& value) const {
    if(!valid_cursor(value))return failure(Code::invalid,"invalid source cursor");
    if(value.sequence==0)return {};
    ev::ObservationClaim claim;auto result=store.lookup_observation(source.uid,source.epoch,value.sequence,claim);
    if(!result)return result;
    return claim.payload_digest==value.payload_sha256?Result{}:failure(Code::conflict,"cursor does not match canonical source history");
  }
  Result floor(const SourceRef& ref,Floor& out,ev::StoredRecord& stored) const {
    const auto result=load(floor_kind,source_key(ref),out,stored);
    if(result.code==Code::not_found){out={};return {};}return result;
  }
  Result replay(const SourceRef& ref,std::uint64_t after) const {
    Source source_;ev::StoredRecord source_record;
    if(auto result=source(ref,source_,source_record);!result)return result;
    if(after>source_.last_sequence)return failure(Code::conflict,"cursor exceeds source history");
    Floor current;ev::StoredRecord record;if(auto result=floor(ref,current,record);!result)return result;
    if(after>=current.next-1)return {};
    // Distinct historical intervals preserve an expired earlier gap even when
    // a later interval is archived. One mutable availability flag loses that fact.
    std::vector<ev::StoredRecord> ranges;bool more=false;
    if(auto result=store.scan_latest(range_kind,source_key(ref)+":",range_key(ref,after),1,ranges,more);!result)return result;
    if(ranges.empty()||!decode(ranges.front().record.payload,current)||current.next-1<=after)
      return bad("retention floor has no indexed range witness");
    if(current.availability==HistoryAvailability::archived){
      if(!archive||archive->identifier()!=current.archive)return failure(Code::archive_unavailable,"required history archive is unavailable");
      if(current.tip.sequence){Bytes boundary;auto result=store.get_blob(current.tip.payload_sha256,boundary);
        if(result.code==Code::history_expired)result=archive->get(current.tip.payload_sha256,boundary);
        if(!result)return result.code==Code::archive_unavailable?result:failure(result.code,"archive replay boundary cannot be verified: "+result.message);
        if(sha256(boundary)!=current.tip.payload_sha256)return bad("archive replay boundary digest mismatch");
      }
      return failure(Code::archive_required,"replay requires the verified archive or an explicit bootstrap");
    }
    return failure(Code::history_expired,"history expired; authorize a new-generation bootstrap");
  }
  Result ensure_no_blocking_subscription(const Active& active,const MaintenanceRequest& request,
                                         std::uint64_t now,ev::CommitBatch* batch=nullptr) const {
    for(const auto& id:active.subscriptions){
      Subscription subscription;ev::StoredRecord stored;
      if(auto result=load(subscription_kind,sha256(id),subscription,stored);!result)return result;
      if(subscription.phase!=SubscriptionPhase::active||subscription.lease_until_unix_s<=now)continue;
      if(subscription.scope==request.source&&subscription.acknowledged.sequence<request.expire_through)
        return failure(Code::busy,"an active subscription still needs the requested history");
      if(batch)guard(*batch,subscription_kind,sha256(id),stored);
    }
    return {};
  }
  Result ensure_unpinned(const Active& active,const MaintenanceRequest& request) const {
    for(const auto& id:active.pins){Pin pin;ev::StoredRecord stored;
      if(auto result=load(pin_kind,sha256(id),pin,stored);!result)return result;
      // A lease deadline never releases a live reader's resource ownership.
      // Every pin remains protective until its explicit release commits.
      for(const auto& object:request.objects)if(contains(pin.objects,object))
        return failure(Code::busy,"a protected root or reader lease retains an object");
    }return {};
  }
  Result ownership(const MaintenanceRequest& request,const Digest& digest) const {
    ev::StoredRecord stored;bool found=false;
    if(auto result=read(object_kind,source_key(request.source)+":"+digest,stored,found);!result)return result;
    if(!found||stored.record.tombstone)return failure(Code::unauthorized,"object is not part of this source history");
    Reader reader(stored.record.payload,"source-object/1");const auto last_sequence=reader.value();
    if(!reader.done())return bad("invalid source object witness");
    return last_sequence<=request.expire_through?Result{}:failure(Code::conflict,"object extends beyond the requested expiry boundary");
  }
};

Service::Service(ev::ICommitStore& store,Options options,IArchiveStore* archive)
  :impl_(std::make_unique<Impl>(store,options,archive)){}
Service::~Service()=default;
Bytes encode_source_observation(const SourceRef& source,std::uint64_t sequence,const Observation& observation) {
  return pack({"source-observation/1",encode(source),number(sequence),encode_observation(observation)});
}
Result decode_source_observation(std::string_view bytes,SourceRef& source,std::uint64_t& sequence,Observation& observation) {
  Reader reader(bytes,"source-observation/1");reader.valid=decode(reader.text(),source)&&reader.valid;
  sequence=reader.value();reader.valid=decode_observation(reader.text(),observation).ok()&&reader.valid;
  return reader.done()&&sequence>0?Result{}:bad("invalid source observation envelope");
}
Result Service::open(){
  const auto& options=impl_->options;
  if(options.max_active_sources==0||options.max_metrics==0||options.max_active_peers==0||
     options.max_active_subscriptions==0||options.max_pins==0||options.max_pin_objects==0||
     options.maximum_lease_seconds==0)return failure(Code::invalid,"lifecycle bounds must be nonzero");
  Active active;ev::StoredRecord record;if(auto result=impl_->active(active,record);!result)return result;
  for(const auto& expected:active.view.sources){Source actual;ev::StoredRecord stored;
    if(auto result=impl_->source(expected.id,actual,stored);!result)return result;
    if(encode(actual)!=encode(expected))return bad("active checkpoint disagrees with source catalog");
    if(auto result=impl_->cursor(actual.id,{actual.last_sequence,actual.last_payload_sha256});!result)return result;
    if(!actual.state_object_sha256.empty()){ev::BlobInfo info;
      if(auto result=impl_->store.blob_info(actual.state_object_sha256,info);!result)return result;
      if(!info.present||info.expired_generation)return bad("active source checkpoint is unavailable");
    }
  }
  for(const auto& metric:active.view.metrics)if(std::none_of(active.view.sources.begin(),active.view.sources.end(),
      [&](const auto& source){return source.id==metric.source&&metric.sequence<=source.last_sequence;}))
    return bad("active metric has no matching source tip");
  for(const auto& id:active.pins){Pin pin;ev::StoredRecord stored;
    if(auto result=impl_->load(pin_kind,sha256(id),pin,stored);!result)return result;
    if(pin.id!=id||pin.objects.size()>impl_->options.max_pin_objects||!unique_digests(pin.objects)||
       stored.record.references!=pin.objects)return bad("invalid protected root directory entry");
  }
  for(const auto& id:active.peers){Peer peer;ev::StoredRecord stored;
    if(auto result=impl_->load(peer_kind,sha256(id),peer,stored);!result)return result;
    if(peer.principal!=id||peer.phase!=PeerPhase::active)return bad("invalid active peer directory entry");
  }
  for(const auto& id:active.subscriptions){Subscription sub;ev::StoredRecord stored;
    if(auto result=impl_->load(subscription_kind,sha256(id),sub,stored);!result)return result;
    if(sub.id!=id||sub.phase!=SubscriptionPhase::active||!contains(active.peers,sub.principal))
      return bad("invalid active subscription directory entry");
    if(auto result=impl_->cursor(sub.scope,sub.acknowledged);!result)return result;
    if(sub.has_offer)if(auto result=impl_->cursor(sub.scope,sub.offered);!result)return result;
  }
  return {};
}
Result Service::snapshot(Snapshot& out) const {
  Active active;ev::StoredRecord record;const auto result=impl_->active(active,record);
  if(result)out=std::move(active.view);
  return result;
}
Result Service::source(const SourceRef& ref,Source& out) const {ev::StoredRecord record;return impl_->source(ref,out,record);}

Result Service::register_engine(const EngineIdentity& identity,std::string owner,std::string operation) {
  SourceRef ref{identity.source_uid,identity.epoch};
  if(!valid_ref(ref)||!valid_token(owner,256)||!valid_token(identity.instance_id)||
     !valid_token(identity.type_id)||!valid_token(identity.behavior_id)||!valid_token(identity.clock_domain)||
     !valid_digest(identity.module_sha256)||!valid_digest(identity.model_sha256))return failure(Code::invalid,"invalid registered engine identity");
  ev::CommitBatch batch;bool duplicate=false;
  if(auto result=impl_->start(batch,std::move(operation),pack({"register/1",encode_identity(identity),owner}),duplicate);!result||duplicate)return result;
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->available(active);!result)return result;
  if(!active.restore_id.empty()&&!identity.source_uid.starts_with(active.restore_id+"."))
    return failure(Code::unauthorized,"new recovery sources must use the current incarnation prefix");
  if(std::any_of(active.view.sources.begin(),active.view.sources.end(),[&](const auto& source){return source.identity.instance_id==identity.instance_id;}))
    return failure(Code::conflict,"engine instance already belongs to another live source");
  if(active.view.sources.size()>=impl_->options.max_active_sources)return failure(Code::capacity,"active source directory is full");
  Source existing;ev::StoredRecord old_source;
  auto source_result=impl_->source(ref,existing,old_source);
  if(source_result.ok())return failure(Code::conflict,"source epoch already exists");
  if(source_result.code!=Code::not_found)return source_result;
  Fence fence;ev::StoredRecord old_fence;const auto fence_key=sha256(ref.uid);
  auto fence_result=impl_->load(fence_kind,fence_key,fence,old_fence);
  if(fence_result.code==Code::not_found){fence.owner=owner;fence.clock=identity.clock_domain;}
  else if(!fence_result)return fence_result;
  if(fence.owner!=owner||fence.clock!=identity.clock_domain)return failure(Code::unauthorized,"source ownership and clock are immutable");
  if(fence.issued!=fence.closed||fence.issued==std::numeric_limits<std::uint64_t>::max()||ref.epoch!=fence.issued+1)
    return failure(Code::conflict,"source epochs must advance after the preceding epoch is retired");
  fence.issued=ref.epoch;
  Source source;source.id=ref;source.identity=identity;source.owner=std::move(owner);
  active.view.sources.push_back(source);
  guard(batch,source_kind,source_key(ref),old_source);put(batch,source_kind,source_key(ref),encode(source));
  guard(batch,fence_kind,fence_key,old_fence);put(batch,fence_kind,fence_key,encode(fence));
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}

namespace {
template<class Implementation>
Result phase_change(Implementation& impl,const SourceRef& ref,SourcePhase from,SourcePhase to,
                    const Cursor* tip,std::string operation) {
  ev::CommitBatch batch;bool duplicate=false;
  if(to!=SourcePhase::active)batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl.start(batch,std::move(operation),pack({"source-phase/1",encode(ref),
      number(static_cast<std::uint64_t>(from)),number(static_cast<std::uint64_t>(to)),tip?encode(*tip):Bytes{}}),duplicate);!result||duplicate)return result;
  Active active;ev::StoredRecord previous;if(auto result=impl.active(active,previous);!result)return result;
  if(auto result=impl.available(active);!result)return result;
  Source source;ev::StoredRecord stored;if(auto result=impl.source(ref,source,stored);!result)return result;
  if(source.phase!=from)return failure(Code::conflict,"source lifecycle transition is not allowed from this phase");
  if(tip&&*tip!=Cursor{source.last_sequence,source.last_payload_sha256})return failure(Code::conflict,"source seal tip changed");
  source.phase=to;auto found=std::find_if(active.view.sources.begin(),active.view.sources.end(),[&](const auto& v){return v.id==ref;});
  if(found==active.view.sources.end())return bad("source is absent from active directory");
  if(to==SourcePhase::retired){
    active.view.sources.erase(found);
    active.view.metrics.erase(std::remove_if(active.view.metrics.begin(),active.view.metrics.end(),
      [&](const auto& v){return v.source==ref;}),active.view.metrics.end());
    Fence fence;ev::StoredRecord fence_record;const auto key=sha256(ref.uid);
    if(auto result=impl.load(fence_kind,key,fence,fence_record);!result)return result;
    if(fence.issued!=ref.epoch||fence.closed+1!=ref.epoch)return bad("source retirement fence mismatch");
    fence.closed=ref.epoch;guard(batch,fence_kind,key,fence_record);put(batch,fence_kind,key,encode(fence));
    if(!add(active.protected_revision))return failure(Code::capacity,"protection revision exhausted");
  }else *found=source;
  guard(batch,source_kind,source_key(ref),stored);put(batch,source_kind,source_key(ref),encode(source),source_references(source));
  if(auto result=impl.save_active(batch,active,previous);!result)return result;
  return impl.commit(batch);
}
}
Result Service::activate(const SourceRef& ref,std::string op){return phase_change(*impl_,ref,SourcePhase::registered,SourcePhase::active,nullptr,std::move(op));}
Result Service::begin_drain(const SourceRef& ref,std::string op){return phase_change(*impl_,ref,SourcePhase::active,SourcePhase::draining,nullptr,std::move(op));}
Result Service::seal(const SourceRef& ref,const Cursor& tip,std::string op){return phase_change(*impl_,ref,SourcePhase::draining,SourcePhase::sealed,&tip,std::move(op));}
Result Service::retire(const SourceRef& ref,std::string op){return phase_change(*impl_,ref,SourcePhase::sealed,SourcePhase::retired,nullptr,std::move(op));}

Result Service::stage_commit(ev::CommitBatch& batch,const PreparedTransition& transition) {
  if(batch.operation_id!=transition.operation_id||batch.request_digest!=transition.request_sha256)
    return failure(Code::conflict,"lifecycle and engine transaction identities differ");
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->available(active);!result)return result;
  SourceRef ref{transition.identity.source_uid,transition.identity.epoch};Source source;ev::StoredRecord stored;
  if(auto result=impl_->source(ref,source,stored);!result)return result;
  if(source.phase!=SourcePhase::active)return failure(Code::expired,"source is no longer admitting work");
  const bool initial=source.state_sha256.empty();
  if(initial&&(transition.expected_revision!=transition.target.revision||
       transition.expected_state_sha256!=state_digest(transition.target)||!transition.observations.empty()))
    return failure(Code::conflict,"initial attachment must bind the exact observation-free checkpoint");
  if(source.identity!=transition.identity||(!initial&&source.engine_revision!=transition.expected_revision)||
     (!initial&&(transition.expected_revision==std::numeric_limits<std::uint64_t>::max()||
       transition.target.revision!=transition.expected_revision+1))||transition.target.tick<source.last_tick||
     (!source.state_sha256.empty()&&source.state_sha256!=transition.expected_state_sha256))
    return failure(Code::conflict,"transition does not continue the pinned source state");
  const auto receipt_bytes=encode_transition(transition),receipt_digest=sha256(receipt_bytes);
  if(auto result=add_blob(batch,receipt_digest,receipt_bytes);!result)return result;
  const auto state_bytes=encode_checkpoint(transition.identity,transition.target),state_hash=state_digest(transition.target);
  const auto state_object=checkpoint_digest(transition.identity,transition.target);
  if(auto result=add_blob(batch,state_object,state_bytes);!result)return result;
  const auto initial_sequence=source.last_sequence;
  std::map<Digest,std::uint64_t> ownership;
  for(const auto& observation:transition.observations){
    if(observation.tick>transition.target.tick||observation.tick<source.last_tick||!valid_token(observation.metric))
      return failure(Code::invalid,"observation clock or metric is invalid");
    if(!add(source.last_sequence)||!add(active.view.lifetime_observations))return failure(Code::capacity,"source sequence exhausted");
    const auto bytes=encode_source_observation(ref,source.last_sequence,observation),digest=sha256(bytes);
    if(auto result=add_blob(batch,digest,bytes);!result)return result;
    batch.observations.push_back({ref.uid,ref.epoch,source.last_sequence,digest,receipt_digest});
    ownership[digest]=source.last_sequence;source.last_payload_sha256=digest;
    auto metric=std::find_if(active.view.metrics.begin(),active.view.metrics.end(),[&](const auto& v){return v.source==ref&&v.observation.metric==observation.metric;});
    if(metric==active.view.metrics.end()){
      if(active.view.metrics.size()>=impl_->options.max_metrics)return failure(Code::capacity,"active metric directory is full");
      active.view.metrics.push_back({ref,observation,source.last_sequence});
    }else *metric={ref,observation,source.last_sequence};
  }
  ownership[state_object]=source.last_sequence;ownership[receipt_digest]=source.last_sequence;
  for(const auto& [digest,last_sequence]:ownership){
    const auto key=source_key(ref)+":"+digest;ev::StoredRecord old;bool found=false;
    if(auto result=impl_->read(object_kind,key,old,found);!result)return result;
    guard(batch,object_kind,key,old);put(batch,object_kind,key,pack({"source-object/1",number(last_sequence)}));
  }
  source.engine_revision=transition.target.revision;source.last_tick=transition.target.tick;source.state_sha256=state_hash;
  source.state_object_sha256=state_object;
  auto live=std::find_if(active.view.sources.begin(),active.view.sources.end(),[&](const auto& v){return v.id==ref;});
  if(live==active.view.sources.end())return bad("source is missing from the active checkpoint");
  *live=source;
  guard(batch,source_kind,source_key(ref),stored);put(batch,source_kind,source_key(ref),encode(source),source_references(source));
  // This record is a bounded historical witness, not a retained payload root.
  put(batch,"lifecycle.transition",source_key(ref)+":"+number(source.engine_revision),
      pack({"source-transition/2",number(initial_sequence),number(source.last_sequence),receipt_digest,state_hash,state_object}));
  return impl_->save_active(batch,active,previous);
}

Result Service::upsert_peer(const Peer& peer,std::uint64_t expected,std::uint64_t now,std::string operation) {
  if(!valid_token(peer.principal,256)||!valid_digest(peer.certificate_sha256)||expected==std::numeric_limits<std::uint64_t>::max()||
     peer.generation!=expected+1||static_cast<std::uint64_t>(peer.phase)>2)
    return failure(Code::invalid,"invalid peer generation or identity");
  ev::CommitBatch batch;bool duplicate=false;
  if(peer.phase!=PeerPhase::active)batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl_->start(batch,std::move(operation),pack({"upsert-peer/1",encode(peer),number(expected)}),duplicate);!result||duplicate)return result;
  if(peer.phase==PeerPhase::active&&!lease_valid(peer.expires_unix_s,now,impl_->options.maximum_lease_seconds))
    return failure(Code::invalid,"active peer needs a bounded future expiry");
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->available(active);!result)return result;
  if(auto result=impl_->time(active,now);!result)return result;
  Peer old;ev::StoredRecord old_record;auto loaded=impl_->load(peer_kind,sha256(peer.principal),old,old_record);
  if(!loaded&&loaded.code!=Code::not_found)return loaded;
  if((loaded.ok()?old.generation:0)!=expected)return failure(Code::conflict,"peer generation changed");
  if(!active.restore_id.empty()&&loaded.ok()&&!contains(active.peers,peer.principal)&&
      peer.phase==PeerPhase::active&&peer.certificate_sha256==old.certificate_sha256)
    return failure(Code::unauthorized,"restored peers require a fresh credential");
  if(peer.phase==PeerPhase::active&&!contains(active.peers,peer.principal)){
    if(active.peers.size()>=impl_->options.max_active_peers)return failure(Code::capacity,"active peer directory is full");
    active.peers.push_back(peer.principal);
  }else if(peer.phase!=PeerPhase::active)erase(active.peers,peer.principal);
  // Rotation/revocation invalidates all existing subscriptions for this peer.
  auto subscriptions=active.subscriptions;
  for(const auto& id:subscriptions){Subscription sub;ev::StoredRecord sub_record;
    if(auto result=impl_->load(subscription_kind,sha256(id),sub,sub_record);!result)return result;
    if(sub.principal!=peer.principal)continue;
    sub.phase=SubscriptionPhase::suspended;sub.has_offer=false;sub.offered={};erase(active.subscriptions,id);
    guard(batch,subscription_kind,sha256(id),sub_record);put(batch,subscription_kind,sha256(id),encode(sub));
    ev::StoredRecord bootstrap;bool found=false;
    if(auto result=impl_->read("lifecycle.bootstrap",sha256(id),bootstrap,found);!result)return result;
    if(found&&!bootstrap.record.tombstone){guard(batch,"lifecycle.bootstrap",sha256(id),bootstrap);put(batch,"lifecycle.bootstrap",sha256(id),{}, {},true);}
  }
  if(!add(active.subscription_revision))return failure(Code::capacity,"subscription revision exhausted");
  guard(batch,peer_kind,sha256(peer.principal),old_record);put(batch,peer_kind,sha256(peer.principal),encode(peer));
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::peer(std::string_view principal,Peer& out) const {
  ev::StoredRecord record;auto result=impl_->load(peer_kind,sha256(principal),out,record);if(!result)return result;
  if(out.principal!=principal)return bad("peer identity mismatch");
  Active active;if(auto status=impl_->active(active,record);!status)return status;
  if(active.quarantined||!contains(active.peers,principal))out.phase=PeerPhase::revoked;
  return {};
}
Result Service::subscription(std::string_view id,Subscription& out) const {
  ev::StoredRecord record;auto result=impl_->load(subscription_kind,sha256(id),out,record);if(!result)return result;
  if(out.id!=id)return bad("subscription identity mismatch");
  Active active;if(auto status=impl_->active(active,record);!status)return status;
  if((active.quarantined||!contains(active.subscriptions,id))&&out.phase==SubscriptionPhase::active)out.phase=SubscriptionPhase::expired;
  return {};
}
Result Service::open_subscription(const Subscription& requested,std::uint64_t now,std::string operation) {
  if(!valid_token(requested.id,256)||!valid_token(requested.principal,256)||!valid_ref(requested.scope)||
     requested.phase!=SubscriptionPhase::active||requested.has_offer||requested.offered!=Cursor{})
    return failure(Code::invalid,"invalid subscription request");
  ev::CommitBatch batch;bool duplicate=false;
  // The acknowledged cursor is a continuity precondition, never a requested
  // mutation. Later ACK progress must not change an old renewal's identity.
  if(auto result=impl_->start(batch,std::move(operation),pack({"open-subscription/1",requested.id,requested.principal,
      encode(requested.scope),number(requested.generation),number(requested.lease_until_unix_s)}),duplicate);!result||duplicate)return result;
  if(!lease_valid(requested.lease_until_unix_s,now,impl_->options.maximum_lease_seconds))
    return failure(Code::invalid,"subscription needs a bounded future lease");
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->time(active,now);!result)return result;
  if(auto result=impl_->authority(active,requested.principal,now,&batch,source_key(requested.scope),requested.lease_until_unix_s);!result)return result;
  Subscription old;ev::StoredRecord old_record;const auto key=sha256(requested.id);
  auto loaded=impl_->load(subscription_kind,key,old,old_record);if(!loaded&&loaded.code!=Code::not_found)return loaded;
  const auto prior=loaded.ok()?old.generation:0;
  if(prior==std::numeric_limits<std::uint64_t>::max()||requested.generation!=prior+1)
    return failure(Code::conflict,"subscription generation must advance exactly once");
  if(loaded.ok()&&(old.principal!=requested.principal||old.scope!=requested.scope))return failure(Code::unauthorized,"subscription principal and scope are immutable");
  if(requested.acknowledged!=(loaded.ok()?old.acknowledged:Cursor{}))return failure(Code::conflict,"ordinary subscription renewal cannot jump its cursor");
  if(!active.restore_id.empty()&&loaded.ok()&&!contains(active.subscriptions,requested.id))
    return failure(Code::rebootstrap_required,"restored subscription requires an explicit new-generation bootstrap");
  if(auto result=impl_->replay(requested.scope,requested.acknowledged.sequence);!result)return result;
  if(!contains(active.subscriptions,requested.id)){
    if(active.subscriptions.size()>=impl_->options.max_active_subscriptions)return failure(Code::capacity,"active subscription directory is full");
    active.subscriptions.push_back(requested.id);
  }
  if(!add(active.subscription_revision))return failure(Code::capacity,"subscription revision exhausted");
  guard(batch,subscription_kind,key,old_record);put(batch,subscription_kind,key,encode(requested));
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}

namespace {
template<class Implementation>
Result subscription_cursor(Implementation& impl,std::string_view id,std::uint64_t generation,const Cursor& cursor,
                           std::uint64_t now,std::string operation,bool acknowledgement) {
  ev::CommitBatch batch;bool duplicate=false;
  if(auto result=impl.start(batch,std::move(operation),pack({acknowledgement?"ack/1":"offer/1",std::string(id),number(generation),encode(cursor)}),duplicate);!result||duplicate)return result;
  Active active;ev::StoredRecord previous;if(auto result=impl.active(active,previous);!result)return result;
  if(auto result=impl.time(active,now);!result)return result;
  Subscription sub;ev::StoredRecord record;const auto key=sha256(id);
  if(auto result=impl.load(subscription_kind,key,sub,record);!result)return result;
  if(sub.id!=id||sub.generation!=generation)return failure(Code::conflict,"stale subscription generation");
  if(!contains(active.subscriptions,id)||sub.phase!=SubscriptionPhase::active||sub.lease_until_unix_s<=now)
    return failure(Code::expired,"subscription is not active or its lease expired");
  if(auto result=impl.authority(active,sub.principal,now,&batch,source_key(sub.scope),sub.lease_until_unix_s);!result)return result;
  if(auto result=impl.cursor(sub.scope,cursor);!result)return result;
  if(acknowledgement){
    if(cursor!=sub.acknowledged&&(!sub.has_offer||cursor!=sub.offered))
      return failure(Code::conflict,"acknowledgement does not match an offered canonical cursor");
    sub.acknowledged=cursor;
    if(sub.has_offer&&cursor==sub.offered){sub.offered={};sub.has_offer=false;}
  }else{
    if(cursor.sequence<sub.acknowledged.sequence)return failure(Code::conflict,"offer cannot regress the acknowledged cursor");
    if(auto result=impl.replay(sub.scope,sub.acknowledged.sequence);!result)return result;
    if(sub.has_offer&&sub.offered!=cursor)return failure(Code::busy,"acknowledge the outstanding offer before replacing it");
    sub.offered=cursor;sub.has_offer=true;
  }
  if(!add(active.subscription_revision))return failure(Code::capacity,"subscription revision exhausted");
  guard(batch,subscription_kind,key,record);put(batch,subscription_kind,key,encode(sub));
  if(auto result=impl.save_active(batch,active,previous);!result)return result;
  return impl.commit(batch);
}
}
Result Service::offer(std::string_view id,std::uint64_t gen,const Cursor& cursor,std::uint64_t now,std::string op){return subscription_cursor(*impl_,id,gen,cursor,now,std::move(op),false);}
Result Service::acknowledge(std::string_view id,std::uint64_t gen,const Cursor& cursor,std::uint64_t now,std::string op){return subscription_cursor(*impl_,id,gen,cursor,now,std::move(op),true);}
Result Service::close_subscription(std::string_view id,std::uint64_t generation,std::string operation) {
  ev::CommitBatch batch;bool duplicate=false;
  batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl_->start(batch,std::move(operation),pack({"close-subscription/1",std::string(id),number(generation)}),duplicate);!result||duplicate)return result;
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  Subscription sub;ev::StoredRecord record;const auto key=sha256(id);
  if(auto result=impl_->load(subscription_kind,key,sub,record);!result)return result;
  if(sub.generation!=generation)return failure(Code::conflict,"stale subscription generation");
  sub.phase=SubscriptionPhase::closed;sub.has_offer=false;sub.offered={};erase(active.subscriptions,id);
  if(!add(active.subscription_revision))return failure(Code::capacity,"subscription revision exhausted");
  guard(batch,subscription_kind,key,record);put(batch,subscription_kind,key,encode(sub));
  // A bootstrap snapshot is retained only for this subscription generation.
  ev::StoredRecord bootstrap;bool found=false;
  if(auto result=impl_->read("lifecycle.bootstrap",key,bootstrap,found);!result)return result;
  if(found&&!bootstrap.record.tombstone){guard(batch,"lifecycle.bootstrap",key,bootstrap);put(batch,"lifecycle.bootstrap",key,{}, {},true);}
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::expire_subscriptions(std::uint64_t now,std::string operation) {
  ev::CommitBatch batch;bool duplicate=false;
  batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl_->start(batch,std::move(operation),pack({"expire-subscriptions/1"}),duplicate);!result||duplicate)return result;
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->time(active,now);!result)return result;
  const auto ids=active.subscriptions;
  for(const auto& id:ids){Subscription sub;ev::StoredRecord record;const auto key=sha256(id);
    if(auto result=impl_->load(subscription_kind,key,sub,record);!result)return result;
    auto authority=impl_->authority(active,sub.principal,now);
    if(sub.lease_until_unix_s>now&&authority.ok())continue;
    if(!authority&&authority.code!=Code::unauthorized)return authority;
    sub.phase=SubscriptionPhase::expired;sub.has_offer=false;sub.offered={};erase(active.subscriptions,id);
    guard(batch,subscription_kind,key,record);put(batch,subscription_kind,key,encode(sub));
    ev::StoredRecord bootstrap;bool found=false;if(auto result=impl_->read("lifecycle.bootstrap",key,bootstrap,found);!result)return result;
    if(found&&!bootstrap.record.tombstone){guard(batch,"lifecycle.bootstrap",key,bootstrap);put(batch,"lifecycle.bootstrap",key,{}, {},true);}
  }
  if(!add(active.subscription_revision))return failure(Code::capacity,"subscription revision exhausted");
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::expire_peer_leases(std::uint64_t now,std::string operation) {
  ev::CommitBatch batch;bool duplicate=false;
  batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl_->start(batch,std::move(operation),pack({"expire-peer-leases/1"}),duplicate);!result||duplicate)return result;
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->time(active,now);!result)return result;
  std::set<std::string> expired;
  for(const auto& principal:active.peers){Peer peer;ev::StoredRecord record;const auto key=sha256(principal);
    if(auto result=impl_->load(peer_kind,key,peer,record);!result)return result;
    if(peer.expires_unix_s>now)continue;
    peer.phase=PeerPhase::suspended;expired.insert(principal);
    guard(batch,peer_kind,key,record);put(batch,peer_kind,key,encode(peer));
  }
  for(const auto& principal:expired)erase(active.peers,principal);
  const auto subscriptions=active.subscriptions;
  for(const auto& id:subscriptions){Subscription sub;ev::StoredRecord record;const auto key=sha256(id);
    if(auto result=impl_->load(subscription_kind,key,sub,record);!result)return result;
    if(!expired.contains(sub.principal))continue;
    sub.phase=SubscriptionPhase::expired;sub.has_offer=false;sub.offered={};erase(active.subscriptions,id);
    guard(batch,subscription_kind,key,record);put(batch,subscription_kind,key,encode(sub));
    ev::StoredRecord bootstrap;bool found=false;
    if(auto result=impl_->read("lifecycle.bootstrap",key,bootstrap,found);!result)return result;
    if(found&&!bootstrap.record.tombstone){guard(batch,"lifecycle.bootstrap",key,bootstrap);put(batch,"lifecycle.bootstrap",key,{}, {},true);}
  }
  if(!add(active.subscription_revision)||!add(active.protected_revision))return failure(Code::capacity,"lifecycle revision exhausted");
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}

Result Service::put_pin(const Pin& pin,std::uint64_t expected,std::uint64_t now,std::string operation) {
  if(!valid_token(pin.id,256)||!valid_token(pin.owner,256)||expected==std::numeric_limits<std::uint64_t>::max()||
     pin.generation!=expected+1||pin.objects.empty()||pin.objects.size()>impl_->options.max_pin_objects||
     !unique_digests(pin.objects)||static_cast<std::uint64_t>(pin.kind)>6)
    return failure(Code::invalid,"invalid bounded pin");
  ev::CommitBatch batch;bool duplicate=false;
  if(auto result=impl_->start(batch,std::move(operation),pack({"put-pin/1",encode(pin),number(expected)}),duplicate);!result||duplicate)return result;
  if(pin.expires_unix_s&&!lease_valid(pin.expires_unix_s,now,impl_->options.maximum_lease_seconds))
    return failure(Code::invalid,"pin needs a bounded future lease or an explicit permanent root");
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->time(active,now);!result)return result;
  Pin old;ev::StoredRecord record;const auto key=sha256(pin.id);auto loaded=impl_->load(pin_kind,key,old,record);
  if(!loaded&&loaded.code!=Code::not_found)return loaded;
  // Tombstones retain the prior generation in their payload for strict renewal.
  if(loaded.code==Code::not_found&&record.revision&&!record.record.payload.empty()){
    if(!decode(record.record.payload,old))return bad("invalid retired pin");
  }
  if((record.revision?old.generation:0)!=expected)return failure(Code::conflict,"pin generation changed");
  if(record.revision&&old.owner!=pin.owner)return failure(Code::unauthorized,"pin ownership is immutable");
  for(const auto& object:pin.objects){ev::BlobInfo info;if(auto result=impl_->store.blob_info(object,info);!result)return result;
    if(info.expired_generation)return failure(Code::history_expired,"cannot pin an expired object");
    if(!info.present)return bad("pin object is missing");}
  if(!contains(active.pins,pin.id)){
    if(active.pins.size()>=impl_->options.max_pins)return failure(Code::capacity,"protected root directory is full");
    active.pins.push_back(pin.id);
  }
  if(!add(active.protected_revision))return failure(Code::capacity,"protection revision exhausted");
  guard(batch,pin_kind,key,record);put(batch,pin_kind,key,encode(pin),pin.objects);
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::release_pin(std::string_view id,std::uint64_t generation,std::string operation) {
  ev::CommitBatch batch;bool duplicate=false;
  batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl_->start(batch,std::move(operation),pack({"release-pin/1",std::string(id),number(generation)}),duplicate);!result||duplicate)return result;
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  Pin pin;ev::StoredRecord record;const auto key=sha256(id);if(auto result=impl_->load(pin_kind,key,pin,record);!result)return result;
  if(pin.generation!=generation)return failure(Code::conflict,"pin generation changed");
  erase(active.pins,id);
  if(!add(active.protected_revision))return failure(Code::capacity,"protection revision exhausted");
  guard(batch,pin_kind,key,record);put(batch,pin_kind,key,encode(pin),{},true);
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::pin(std::string_view id,Pin& out) const {ev::StoredRecord record;return impl_->load(pin_kind,sha256(id),out,record);}

Result Service::review_maintenance(const MaintenanceRequest& request,std::string operation,MaintenancePlan& out) {
  if(!valid_token(request.id,256)||!valid_ref(request.source)||request.objects.empty()||
     request.objects.size()>impl_->options.max_pin_objects||!unique_digests(request.objects)||
     request.expire_through==std::numeric_limits<std::uint64_t>::max()||
     (request.disposition!=HistoryAvailability::archived&&request.disposition!=HistoryAvailability::expired))
    return failure(Code::invalid,"invalid maintenance request");
  ev::CommitBatch batch;bool duplicate=false;
  batch.commit_class=ev::CommitClass::maintenance;
  auto identity=request;identity.now_unix_s=0;
  if(auto result=impl_->start(batch,std::move(operation),pack({"review-maintenance/1",encode(identity)}),duplicate);!result)return result;
  if(duplicate)return maintenance(request.id,out);
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->available(active);!result)return result;
  if(auto result=impl_->time(active,request.now_unix_s);!result)return result;
  Source source;ev::StoredRecord source_record;if(auto result=impl_->source(request.source,source,source_record);!result)return result;
  if(source.phase!=SourcePhase::sealed&&source.phase!=SourcePhase::retired)return failure(Code::busy,"seal or retire the source before expiring its history");
  if(request.expire_through>source.last_sequence)return failure(Code::conflict,"expiry exceeds source history");
  if(auto result=impl_->ensure_no_blocking_subscription(active,request,request.now_unix_s);!result)return result;
  if(auto result=impl_->ensure_unpinned(active,request);!result)return result;
  Floor floor;ev::StoredRecord floor_record;if(auto result=impl_->floor(request.source,floor,floor_record);!result)return result;
  if(request.expire_through+1<floor.next)return failure(Code::conflict,"retention floor cannot regress");
  if(request.disposition==HistoryAvailability::archived&&(!impl_->archive||request.archive_directory!=impl_->archive->identifier()))
    return failure(Code::archive_unavailable,"configured archive does not match the requested archive");
  out={};out.request=request;out.protected_revision=active.protected_revision;
  out.subscription_revision=active.subscription_revision;out.source_sequence=source.last_sequence;
  out.source_tip_sha256=source.last_payload_sha256;out.protected_root_sha256=sha256(encode(active));
  out.new_floor=request.expire_through+1;
  for(const auto& object:request.objects){
    if(auto result=impl_->ownership(request,object);!result)return result;
    ev::BlobInfo info;if(auto result=impl_->store.blob_info(object,info);!result)return result;
    if(info.expired_generation)return failure(Code::history_expired,"maintenance object already expired");
    if(!info.present)return bad("maintenance object is missing");
    if(!add(out.expected_bytes,info.bytes))return failure(Code::capacity,"maintenance byte count overflow");
    if(request.disposition==HistoryAvailability::archived){Bytes bytes,verified;
      if(auto result=impl_->store.get_blob(object,bytes);!result)return result;
      if(auto result=impl_->archive->put(object,bytes);!result)return result;
      if(auto result=impl_->archive->get(object,verified);!result)return result;
      if(verified!=bytes||sha256(verified)!=object)return bad("archive read-back did not match the source object");
    }
  }
  ev::StoredRecord old;bool found=false;const auto key=sha256(request.id);
  if(auto result=impl_->read(maintenance_kind,key,old,found);!result)return result;
  if(found)return failure(Code::conflict,"maintenance id already exists");
  guard(batch,maintenance_kind,key,old);put(batch,maintenance_kind,key,encode(out));
  guard(batch,source_kind,source_key(request.source),source_record);
  guard(batch,floor_kind,source_key(request.source),floor_record);
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::maintenance(std::string_view id,MaintenancePlan& out) const {ev::StoredRecord record;return impl_->load(maintenance_kind,sha256(id),out,record);}
Result Service::activate_maintenance(std::string_view id,std::uint64_t now,std::string operation) {
  ev::CommitBatch batch;bool duplicate=false;
  batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl_->start(batch,std::move(operation),pack({"activate-maintenance/1",std::string(id)}),duplicate);!result||duplicate)return result;
  MaintenancePlan plan;ev::StoredRecord plan_record;const auto key=sha256(id);
  if(auto result=impl_->load(maintenance_kind,key,plan,plan_record);!result)return result;
  if(plan.phase!=MaintenancePhase::reviewed)return failure(Code::conflict,"maintenance is no longer in review");
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->available(active);!result)return result;
  if(auto result=impl_->time(active,now);!result)return result;
  if(active.protected_revision!=plan.protected_revision||active.subscription_revision!=plan.subscription_revision)
    return failure(Code::conflict,"protected roots or subscriptions changed after maintenance review");
  Source source;ev::StoredRecord source_record;if(auto result=impl_->source(plan.request.source,source,source_record);!result)return result;
  if((source.phase!=SourcePhase::sealed&&source.phase!=SourcePhase::retired)||source.last_sequence!=plan.source_sequence||
     source.last_payload_sha256!=plan.source_tip_sha256)return failure(Code::conflict,"source changed after maintenance review");
  if(auto result=impl_->ensure_unpinned(active,plan.request);!result)return result;
  if(auto result=impl_->ensure_no_blocking_subscription(active,plan.request,now,&batch);!result)return result;
  if(plan.request.disposition==HistoryAvailability::archived){
    if(!impl_->archive||impl_->archive->identifier()!=plan.request.archive_directory)return failure(Code::archive_unavailable,"reviewed archive is unavailable");
    for(const auto& object:plan.request.objects){Bytes bytes;
      if(auto result=impl_->archive->get(object,bytes);!result)return result;
      if(sha256(bytes)!=object)return bad("archived object changed since review");
    }
  }
  Floor current;ev::StoredRecord floor_record;if(auto result=impl_->floor(plan.request.source,current,floor_record);!result)return result;
  if(plan.new_floor<current.next)return failure(Code::conflict,"retention floor advanced after review");
  Cursor boundary;
  if(plan.request.expire_through){ev::ObservationClaim claim;
    if(auto result=impl_->store.lookup_observation(plan.request.source.uid,plan.request.source.epoch,plan.request.expire_through,claim);!result)return result;
    boundary={claim.sequence,claim.payload_digest};
  }
  Floor updated{plan.new_floor,plan.request.disposition,plan.request.archive_directory,boundary};
  ev::StoredRecord range_record;bool range_exists=false;
  const auto range_id=range_key(plan.request.source,plan.request.expire_through);
  if(auto result=impl_->read(range_kind,range_id,range_record,range_exists);!result)return result;
  Floor range=updated;
  if(range_exists){Floor prior;if(!decode(range_record.record.payload,prior))return bad("invalid retention range witness");
    if(prior.availability==HistoryAvailability::expired)range.availability=HistoryAvailability::expired;
    else if(range.availability==HistoryAvailability::archived&&prior.archive!=range.archive)
      return failure(Code::conflict,"one retained interval cannot silently switch archive identity");
  }
  // The prune manifest and replay floor become durable BEFORE physical deletion.
  plan.phase=MaintenancePhase::activated;
  guard(batch,maintenance_kind,key,plan_record);put(batch,maintenance_kind,key,encode(plan));
  guard(batch,source_kind,source_key(plan.request.source),source_record);
  guard(batch,floor_kind,source_key(plan.request.source),floor_record);put(batch,floor_kind,source_key(plan.request.source),encode(updated));
  guard(batch,range_kind,range_id,range_record);put(batch,range_kind,range_id,encode(range));
  batch.expire_blobs=plan.request.objects;
  if(!add(active.protected_revision))return failure(Code::capacity,"protection revision exhausted");
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::execute_maintenance(std::string_view id,std::string operation) {
  ev::CommitBatch batch;bool duplicate=false;
  batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl_->start(batch,std::move(operation),pack({"execute-maintenance/1",std::string(id)}),duplicate);!result||duplicate)return result;
  MaintenancePlan plan;ev::StoredRecord record;const auto key=sha256(id);
  if(auto result=impl_->load(maintenance_kind,key,plan,record);!result)return result;
  if(plan.phase==MaintenancePhase::reviewed)return failure(Code::conflict,"maintenance must be activated before physical reclamation");
  if(plan.phase==MaintenancePhase::completed)return failure(Code::completed,"maintenance already completed");
  for(const auto& object:plan.request.objects){ev::BlobInfo info;
    if(auto result=impl_->store.blob_info(object,info);!result)return result;
    if(!info.expired_generation)return bad("manifest object lacks canonical expiry");
    if(info.present)if(auto result=impl_->store.reclaim_expired_blob(object,impl_->store.generation());!result)return result;
  }
  plan.phase=MaintenancePhase::completed;guard(batch,maintenance_kind,key,record);put(batch,maintenance_kind,key,encode(plan));
  return impl_->commit(batch);
}
Result Service::replay_status(const SourceRef& ref,std::uint64_t after) const {return impl_->replay(ref,after);}

Result Service::accept_bootstrap(const Bootstrap& request,std::uint64_t now,std::string operation) {
  if(!valid_token(request.subscription_id,256)||!valid_token(request.principal,256)||!valid_ref(request.scope)||
     !valid_cursor(request.tip)||!valid_digest(request.snapshot_sha256)||sha256(request.snapshot)!=request.snapshot_sha256)
    return failure(Code::invalid,"invalid bootstrap identity, snapshot or lease");
  EngineState state;if(auto result=decode_state(request.snapshot,state);!result)return result;
  if(encode_state(state)!=request.snapshot)return failure(Code::corrupt,"bootstrap snapshot is not canonical");
  ev::CommitBatch batch;bool duplicate=false;
  if(auto result=impl_->start(batch,std::move(operation),pack({"accept-bootstrap/1",request.subscription_id,request.principal,
      number(request.generation),number(request.lease_until_unix_s),encode(request.scope),encode(request.tip),request.snapshot_sha256}),duplicate);!result||duplicate)return result;
  if(!lease_valid(request.lease_until_unix_s,now,impl_->options.maximum_lease_seconds))
    return failure(Code::invalid,"bootstrap needs a bounded future lease");
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(auto result=impl_->time(active,now);!result)return result;
  if(auto result=impl_->authority(active,request.principal,now,&batch,source_key(request.scope),request.lease_until_unix_s);!result)return result;
  Source source;ev::StoredRecord source_record;if(auto result=impl_->source(request.scope,source,source_record);!result)return result;
  if(request.tip!=Cursor{source.last_sequence,source.last_payload_sha256}||request.snapshot_sha256!=source.state_sha256||
     state.revision!=source.engine_revision||state.tick!=source.last_tick)
    return failure(Code::conflict,"bootstrap does not match the canonical source snapshot and tip");
  const auto state_object=checkpoint_digest(source.identity,state);
  if(state_object!=source.state_object_sha256)return failure(Code::conflict,"bootstrap checkpoint object identity differs from the source");
  Subscription old;ev::StoredRecord old_record;const auto key=sha256(request.subscription_id);
  auto loaded=impl_->load(subscription_kind,key,old,old_record);if(!loaded&&loaded.code!=Code::not_found)return loaded;
  const auto prior=loaded.ok()?old.generation:0;
  if(prior==std::numeric_limits<std::uint64_t>::max()||request.generation!=prior+1)
    return failure(Code::conflict,"bootstrap requires the next explicit subscription generation");
  if(loaded.ok()&&(old.principal!=request.principal||old.scope!=request.scope||old.acknowledged.sequence>request.tip.sequence))
    return failure(Code::conflict,"bootstrap breaks subscription identity or prior continuity");
  if(!contains(active.subscriptions,request.subscription_id)){
    if(active.subscriptions.size()>=impl_->options.max_active_subscriptions)return failure(Code::capacity,"active subscription directory is full");
    active.subscriptions.push_back(request.subscription_id);
  }
  ev::BlobInfo info;auto blob_status=impl_->store.blob_info(state_object,info);
  if(blob_status.ok()&&info.expired_generation)return failure(Code::history_expired,"bootstrap snapshot expired; a new source snapshot is required");
  if(!blob_status&&blob_status.code!=Code::not_found)return blob_status;
  if(auto result=add_blob(batch,state_object,encode_checkpoint(source.identity,state));!result)return result;
  Subscription sub{request.subscription_id,request.principal,request.scope,request.generation,request.lease_until_unix_s,
    SubscriptionPhase::active,request.tip,{},false};
  guard(batch,subscription_kind,key,old_record);put(batch,subscription_kind,key,encode(sub));
  ev::StoredRecord old_bootstrap;bool found=false;
  if(auto result=impl_->read("lifecycle.bootstrap",key,old_bootstrap,found);!result)return result;
  guard(batch,"lifecycle.bootstrap",key,old_bootstrap);
  put(batch,"lifecycle.bootstrap",key,pack({"bootstrap/2",encode(sub),request.snapshot_sha256,state_object,number(now)}),{state_object});
  guard(batch,source_kind,source_key(request.scope),source_record);
  if(!add(active.subscription_revision)||!add(active.protected_revision))return failure(Code::capacity,"lifecycle revision exhausted");
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::installed_bootstrap(std::string_view id,Bootstrap& out) const {
  ev::StoredRecord record;bool found=false;
  if(auto result=impl_->read("lifecycle.bootstrap",sha256(id),record,found);!result)return result;
  if(!found||record.record.tombstone)return failure(Code::not_found,"subscription has no installed bootstrap snapshot");
  Reader reader(record.record.payload,"bootstrap/2");Subscription sub;
  reader.valid=decode(reader.text(),sub)&&reader.valid;const auto digest=reader.text(),object=reader.text();(void)reader.value();
  if(!reader.done()||sub.id!=id||!valid_digest(digest)||!valid_digest(object)||record.record.references!=std::vector<Digest>{object})
    return bad("invalid installed bootstrap record");
  Bytes snapshot;if(auto result=impl_->store.get_blob(object,snapshot);!result)return result;
  EngineState state;EngineIdentity identity;
  if(sha256(snapshot)!=object||!decode_checkpoint(snapshot,identity,state)||state_digest(state)!=digest)
    return bad("invalid installed bootstrap state");
  Source source;ev::StoredRecord source_record;if(auto result=impl_->source(sub.scope,source,source_record);!result)return result;
  if(identity!=source.identity)return bad("installed bootstrap source identity mismatch");
  snapshot=encode_state(state);
  out={sub.id,sub.principal,sub.generation,sub.lease_until_unix_s,sub.scope,sub.acknowledged,digest,std::move(snapshot)};
  return {};
}

Result Service::quarantine_restore(std::string operation,std::string_view nonce) {
  if(!nonce.empty()&&!valid_digest(nonce))return failure(Code::invalid,"restore incarnation nonce must be a SHA-256 identity");
  ev::CommitBatch batch;bool duplicate=false;
  batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl_->start(batch,std::move(operation),pack({"quarantine-restore/2",std::string(nonce)}),duplicate);!result||duplicate)return result;
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  active.quarantined=true;active.restore_id=sha256(pack({impl_->store.info().store_id,impl_->store.info().root_digest,batch.operation_id,std::string(nonce)}));
  active.recovery_policy_sha256.clear();
  if(!add(active.subscription_revision)||!add(active.protected_revision))return failure(Code::capacity,"lifecycle revision exhausted");
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::reauthorize_restore(std::string_view witness,std::string operation) {
  if(!valid_digest(witness))return failure(Code::invalid,"current operator policy witness must be a SHA-256 digest");
  ev::CommitBatch batch;bool duplicate=false;
  batch.commit_class=ev::CommitClass::maintenance;
  if(auto result=impl_->start(batch,std::move(operation),pack({"reauthorize-restore/1",std::string(witness)}),duplicate);!result||duplicate)return result;
  Active active;ev::StoredRecord previous;if(auto result=impl_->active(active,previous);!result)return result;
  if(!active.quarantined)return failure(Code::conflict,"restore reauthorization requires quarantine");
  std::vector<std::string> retired_identities;
  for(const auto& source:active.view.sources)retired_identities.push_back(encode_identity(source.identity));
  ev::StoredRecord cleanup;bool cleanup_exists=false;const auto cleanup_key=sha256(batch.operation_id);
  if(auto result=impl_->read("lifecycle.restore-cleanup",cleanup_key,cleanup,cleanup_exists);!result)return result;
  if(cleanup_exists)return failure(Code::conflict,"restore cleanup identity already exists without its operation receipt");
  guard(batch,"lifecycle.restore-cleanup",cleanup_key,cleanup);
  put(batch,"lifecycle.restore-cleanup",cleanup_key,
    pack({"restore-cleanup/1",batch.operation_id,active.restore_id,std::string(witness),pack(retired_identities)}));
  for(auto source:active.view.sources){ev::StoredRecord old_source;Source verified;
    if(auto result=impl_->source(source.id,verified,old_source);!result)return result;
    source.phase=SourcePhase::retired;
    guard(batch,source_kind,source_key(source.id),old_source);put(batch,source_kind,source_key(source.id),encode(source));
    Fence fence;ev::StoredRecord old_fence;const auto key=sha256(source.id.uid);
    if(auto result=impl_->load(fence_kind,key,fence,old_fence);!result)return result;
    fence.closed=fence.issued;guard(batch,fence_kind,key,old_fence);put(batch,fence_kind,key,encode(fence));
  }
  for(const auto& id:active.subscriptions){ev::StoredRecord bootstrap;bool found=false;const auto key=sha256(id);
    if(auto result=impl_->read("lifecycle.bootstrap",key,bootstrap,found);!result)return result;
    if(found&&!bootstrap.record.tombstone){guard(batch,"lifecycle.bootstrap",key,bootstrap);put(batch,"lifecycle.bootstrap",key,{}, {},true);}
  }
  // Global authority invalidation is atomic and bounded. Historical peer and
  // subscription records remain queryable but no longer grant any capability.
  active.view.sources.clear();active.view.metrics.clear();active.peers.clear();active.subscriptions.clear();
  active.quarantined=false;active.recovery_policy_sha256=std::string(witness);
  if(!add(active.subscription_revision)||!add(active.protected_revision))return failure(Code::capacity,"lifecycle revision exhausted");
  if(auto result=impl_->save_active(batch,active,previous);!result)return result;
  return impl_->commit(batch);
}
Result Service::reauthorization_sources(std::string_view operation,std::vector<EngineIdentity>& out) const {
  ev::StoredRecord record;bool found=false;
  if(auto result=impl_->read("lifecycle.restore-cleanup",sha256(operation),record,found);!result)return result;
  if(!found||record.record.tombstone)return failure(Code::not_found,"restore reauthorization cleanup has not committed");
  Reader reader(record.record.payload,"restore-cleanup/1");const auto recorded=reader.text();
  const auto incarnation=reader.text(),witness=reader.text();std::vector<std::string> entries;
  reader.valid=unpack(reader.text(),entries,impl_->options.max_active_sources).ok()&&reader.valid;
  if(!reader.done()||recorded!=operation||!valid_digest(incarnation)||!valid_digest(witness))return bad("invalid restore cleanup witness");
  std::vector<EngineIdentity> identities;std::set<std::string> seen;
  for(const auto& bytes:entries){EngineIdentity identity;if(auto result=decode_identity(bytes,identity);!result)return result;
    if(!seen.insert(identity.instance_id).second)return bad("repeated restore cleanup identity");
    identities.push_back(std::move(identity));
  }
  out=std::move(identities);return {};
}

std::string_view phase_name(SourcePhase phase) noexcept {
  switch(phase){case SourcePhase::registered:return "registered";case SourcePhase::active:return "active";
    case SourcePhase::draining:return "draining";case SourcePhase::sealed:return "sealed";case SourcePhase::retired:return "retired";}return "invalid";
}
std::string_view phase_name(SubscriptionPhase phase) noexcept {
  switch(phase){case SubscriptionPhase::active:return "active";case SubscriptionPhase::suspended:return "suspended";
    case SubscriptionPhase::expired:return "expired";case SubscriptionPhase::closed:return "closed";}return "invalid";
}
} // namespace anaiero::sdk::lifecycle
