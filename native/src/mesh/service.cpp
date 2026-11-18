#include "anaiero/sdk/mesh.hpp"
#include <algorithm>
#include <charconv>
#include <limits>
#include <map>
#include <set>

namespace anaiero::sdk::mesh {
namespace ev=evidence;
namespace lc=lifecycle;
namespace {
Result fail(Code code,std::string message){return {code,std::move(message)};}
std::string number(std::uint64_t value){return std::to_string(value);}
bool integer(std::string_view text,std::uint64_t& value){
  if(text.empty())return false;
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&number(value)==text;
}
lc::SourceRef scope(const EngineIdentity& id){return {id.source_uid,id.epoch};}
Bytes encode_scope(const lc::SourceRef& value){return pack({value.uid,number(value.epoch)});}
Bytes encode_cursor(const lc::Cursor& value){return pack({number(value.sequence),value.payload_sha256});}
bool decode_cursor(std::string_view bytes,lc::Cursor& value){
  std::vector<std::string> f;if(!unpack(bytes,f,2,256)||f.size()!=2||!integer(f[0],value.sequence))return false;
  value.payload_sha256=f[1];return value.sequence?valid_digest(value.payload_sha256):value.payload_sha256.empty();
}
std::string grant_key(std::string_view principal,const lc::SourceRef& source){return sha256(pack({std::string(principal),encode_scope(source)}));}
std::string receive_key(std::string_view principal,std::string_view subscription,const lc::SourceRef& source){
  return sha256(pack({std::string(principal),std::string(subscription),encode_scope(source)}));
}
std::string received_observation_key(std::string_view principal,const lc::SourceRef& source,std::uint64_t sequence){
  const auto digits=number(sequence);return grant_key(principal,source)+":"+std::string(20-digits.size(),'0')+digits;
}
std::string operation(std::string_view action,std::string_view bytes){return "mesh2-"+std::string(action)+"-"+sha256(bytes);}
Bytes offer_metadata(const BootstrapOffer& offer){return pack({"mesh-bootstrap-offer/2",offer.id,offer.subscription_id,
  number(offer.generation),number(offer.lease_until_unix_s),encode_identity(offer.source),encode_cursor(offer.tip),offer.checkpoint_sha256});}
void guard(ev::CommitBatch& batch,std::string kind,std::string key,const ev::StoredRecord& old){
  batch.guards.push_back({std::move(kind),std::move(key),old.revision,old.digest});
}
void record(ev::CommitBatch& batch,std::string kind,std::string key,Bytes payload,std::vector<Digest> references={}){
  batch.records.push_back({std::move(kind),std::move(key),std::move(payload),std::move(references),false});
}
Result read(ev::ICommitStore& store,std::string_view kind,std::string_view key,ev::StoredRecord& result,bool& found){
  result={};const auto status=store.latest(kind,key,result);found=status.ok();
  return status.code==Code::not_found?Result{}:status;
}
Result decode_receive(const ev::StoredRecord& record,ReceiveReceipt& out){
  std::vector<std::string> f;
  if(!unpack(record.record.payload,f,6,4096)||f.size()!=6||f[0]!="mesh-receive/2"||
      !integer(f[1],out.generation)||!decode_cursor(f[2],out.cursor)||!valid_digest(f[3]))
    return fail(Code::corrupt,"invalid canonical mesh receive cursor");
  out.receipt_sha256=f[3];out.store_generation=record.generation;return {};
}
Bytes encode_retention_request(const ReceiverRetentionRequest& value){
  return pack({"mesh-receiver-retention/1",value.id,value.principal,value.source.uid,number(value.source.epoch),
    number(value.first_sequence),number(value.last_sequence),pack(value.artifacts),pack(value.bootstrap_ids),pack(value.delivery_receipts)});
}
bool valid_retention_request(const ReceiverRetentionRequest& value){
  if(!valid_token(value.id,256)||!valid_token(value.principal,256)||!valid_token(value.source.uid,256)||!value.source.epoch||
     value.artifacts.size()>8||value.bootstrap_ids.size()>8||value.delivery_receipts.size()>8)return false;
  if(value.first_sequence||value.last_sequence){
    if(!value.first_sequence||value.last_sequence<value.first_sequence||value.last_sequence-value.first_sequence>=32)return false;
  }else if(value.artifacts.empty()&&value.bootstrap_ids.empty()&&value.delivery_receipts.empty())return false;
  std::set<std::string> unique;
  for(const auto& id:value.artifacts)if(!valid_digest(id)||!unique.insert(id).second)return false;
  unique.clear();for(const auto& id:value.bootstrap_ids)if(!valid_token(id,256)||!unique.insert(id).second)return false;
  unique.clear();for(const auto& id:value.delivery_receipts)if(!valid_digest(id)||!unique.insert(id).second)return false;
  return true;
}
Bytes encode_retention_plan(const ReceiverRetentionPlan& value){
  return pack({"mesh-receiver-retention-plan/1",encode_retention_request(value.request),number(static_cast<std::uint64_t>(value.phase)),
    value.store_id,value.reviewed_root,value.candidate_sha256,number(value.reviewed_generation),number(value.expected_bytes),pack(value.objects)});
}
Result decode_retention_plan(std::string_view bytes,ReceiverRetentionPlan& out){
  out={};std::vector<std::string> f,r;std::uint64_t phase=0;
  if(!unpack(bytes,f,9,32768)||f.size()!=9||f[0]!="mesh-receiver-retention-plan/1"||
     !unpack(f[1],r,10,16384)||r.size()!=10||r[0]!="mesh-receiver-retention/1"||
     !integer(r[4],out.request.source.epoch)||!integer(r[5],out.request.first_sequence)||!integer(r[6],out.request.last_sequence)||
     !unpack(r[7],out.request.artifacts,8,4096)||!unpack(r[8],out.request.bootstrap_ids,8,4096)||!unpack(r[9],out.request.delivery_receipts,8,4096)||
     !integer(f[2],phase)||phase>2||!valid_digest(f[3])||!valid_digest(f[4])||!valid_digest(f[5])||
     !integer(f[6],out.reviewed_generation)||!out.reviewed_generation||!integer(f[7],out.expected_bytes)||
     !unpack(f[8],out.objects,64,8192)||out.objects.empty())return fail(Code::corrupt,"invalid receiver retention plan");
  out.request.id=r[1];out.request.principal=r[2];out.request.source.uid=r[3];
  if(!valid_retention_request(out.request))return fail(Code::corrupt,"invalid receiver retention scope");
  std::set<Digest> unique;for(const auto& object:out.objects)if(!valid_digest(object)||!unique.insert(object).second)return fail(Code::corrupt,"invalid receiver retention objects");
  out.phase=static_cast<lc::MaintenancePhase>(phase);out.store_id=f[3];out.reviewed_root=f[4];out.candidate_sha256=f[5];return {};
}
Result retention_retry(ev::ICommitStore& store,std::string_view id,std::string_view request,bool& duplicate){
  duplicate=false;if(!valid_token(id,256))return fail(Code::invalid,"invalid retention operation identity");
  ev::CommitReceipt previous;const auto found=store.lookup_operation(id,previous);
  if(found){if(previous.request_digest!=request)return fail(Code::conflict,"receiver retention operation changed");duplicate=true;return {};}
  return found.code==Code::not_found?Result{}:found;
}
// Carry transport expiry and exact mesh policy revisions through lifecycle's
// own canonical transaction preparation to the actual storage admission clock.
class AuthorizedStore final:public ev::ICommitStore {
  ev::ICommitStore& store_;const ev::CommitBatch& authority_;
public:
  AuthorizedStore(ev::ICommitStore& store,const ev::CommitBatch& authority):store_(store),authority_(authority){}
  Result commit(const ev::CommitBatch& original,ev::CommitReceipt& receipt) override {
    auto batch=original;
    for(const auto& required:authority_.guards){
      const auto existing=std::find_if(batch.guards.begin(),batch.guards.end(),[&](const auto& value){return value.kind==required.kind&&value.key==required.key;});
      if(existing==batch.guards.end())batch.guards.push_back(required);
      else if(existing->expected_revision!=required.expected_revision||existing->expected_digest!=required.expected_digest)
        return fail(Code::conflict,"mesh authority changed during lifecycle preparation");
    }
    batch.authorizations.insert(batch.authorizations.end(),authority_.authorizations.begin(),authority_.authorizations.end());
    return store_.commit(batch,receipt);
  }
  Result lookup_operation(std::string_view id,ev::CommitReceipt& out)override{return store_.lookup_operation(id,out);}
  Result latest(std::string_view kind,std::string_view key,ev::StoredRecord& out)override{return store_.latest(kind,key,out);}
  Result scan_latest(std::string_view kind,std::string_view prefix,std::string_view after,std::size_t count,std::vector<ev::StoredRecord>& out,bool& more)override{return store_.scan_latest(kind,prefix,after,count,out,more);}
  Result blob_info(std::string_view digest,ev::BlobInfo& out)override{return store_.blob_info(digest,out);}
  Result list_blobs(std::uint64_t after,std::size_t limit,std::vector<ev::BlobInfo>& out,bool& more)override{return store_.list_blobs(after,limit,out,more);}
  Result lookup_observation(std::string_view source,std::uint64_t epoch,std::uint64_t sequence,ev::ObservationClaim& out)override{return store_.lookup_observation(source,epoch,sequence,out);}
  Result find_event(const ev::EventKey& key,ev::StoredEvent& out,std::string_view object,std::string_view origin)override{return store_.find_event(key,out,object,origin);}
  std::uint64_t generation()const noexcept override{return store_.generation();}
  Result get_blob(std::string_view digest,Bytes& out)override{return store_.get_blob(digest,out);}
  Result reclaim_expired_blob(std::string_view digest,std::uint64_t generation)override{return store_.reclaim_expired_blob(digest,generation);}
  Result repair_index()override{return store_.repair_index();}
  Result audit(ev::AuditReport& out)override{return store_.audit(out);}
  ev::StoreInfo info()const override{return store_.info();}
};
}

Bytes encode_grant(const Grant& grant){
  std::vector<std::string> f{"mesh-grant/2",grant.principal,encode_identity(grant.source),number(grant.generation),
    grant.send?"1":"0",grant.receive?"1":"0",grant.bootstrap?"1":"0"};
  f.insert(f.end(),grant.artifacts.begin(),grant.artifacts.end());return pack(f);
}
Result decode_grant(std::string_view bytes,Grant& grant){
  std::vector<std::string> f;if(!unpack(bytes,f,39,32768)||f.size()<7||f[0]!="mesh-grant/2"||
      !valid_token(f[1],256)||!decode_identity(f[2],grant.source)||!integer(f[3],grant.generation)||!grant.generation)
    return fail(Code::corrupt,"invalid mesh scope grant");
  for(std::size_t i=4;i<7;++i)if(f[i]!="0"&&f[i]!="1")return fail(Code::corrupt,"invalid mesh grant flag");
  grant.principal=f[1];grant.send=f[4]=="1";grant.receive=f[5]=="1";grant.bootstrap=f[6]=="1";
  grant.artifacts.assign(f.begin()+7,f.end());std::set<Digest> unique;
  for(const auto& digest:grant.artifacts)if(!valid_digest(digest)||!unique.insert(digest).second)
    return fail(Code::corrupt,"invalid mesh artifact grant");
  return {};
}
Bytes encode_delivery(const Delivery& page){
  std::vector<std::string> f{"mesh-page/2",page.subscription_id,number(page.generation),encode_identity(page.source),
    encode_cursor(page.from),encode_cursor(page.to)};
  for(const auto& item:page.observations)f.push_back(pack({number(item.claim.sequence),item.claim.payload_digest,
    item.claim.receipt_digest,item.payload}));
  return pack(f);
}
Result decode_delivery(std::string_view bytes,Delivery& page,const Options& options){
  page={};std::vector<std::string> f;
  if(!unpack(bytes,f,6+options.maximum_page_observations,options.maximum_page_bytes)||f.size()<7||f[0]!="mesh-page/2"||
      !valid_token(f[1],256)||!integer(f[2],page.generation)||!page.generation||!decode_identity(f[3],page.source)||
      !decode_cursor(f[4],page.from)||!decode_cursor(f[5],page.to))return fail(Code::invalid,"invalid bounded mesh page");
  page.subscription_id=f[1];auto sequence=page.from.sequence;
  for(std::size_t i=6;i<f.size();++i){
    std::vector<std::string> item;DeliveryItem value;lc::SourceRef source;std::uint64_t observed_sequence=0;Observation observation;
    if(!unpack(f[i],item,4,options.maximum_page_bytes)||item.size()!=4||!integer(item[0],value.claim.sequence)||
       sequence==std::numeric_limits<std::uint64_t>::max()||value.claim.sequence!=++sequence||
       !valid_digest(item[1])||!valid_digest(item[2])||sha256(item[3])!=item[1]||
       !lc::decode_source_observation(item[3],source,observed_sequence,observation)||source!=scope(page.source)||observed_sequence!=sequence)
      return fail(Code::corrupt,"mesh page observation identity, digest or continuity mismatch");
    value.claim={page.source.source_uid,page.source.epoch,sequence,item[1],item[2]};value.payload=std::move(item[3]);
    page.observations.push_back(std::move(value));
  }
  if(page.to!=lc::Cursor{sequence,page.observations.back().claim.payload_digest})
    return fail(Code::corrupt,"mesh page terminal cursor differs from its bytes");
  return {};
}
Bytes encode_bootstrap(const BootstrapOffer& offer){return pack({"mesh-bootstrap/2",offer.id,offer.subscription_id,
  number(offer.generation),number(offer.lease_until_unix_s),encode_identity(offer.source),encode_cursor(offer.tip),
  offer.checkpoint_sha256,offer.checkpoint});}
Result decode_bootstrap(std::string_view bytes,BootstrapOffer& offer,std::size_t maximum){
  std::vector<std::string> f;
  if(!unpack(bytes,f,9,maximum)||f.size()!=9||f[0]!="mesh-bootstrap/2"||!valid_token(f[1],256)||!valid_token(f[2],256)||
      !integer(f[3],offer.generation)||!offer.generation||!integer(f[4],offer.lease_until_unix_s)||
      !decode_identity(f[5],offer.source)||!decode_cursor(f[6],offer.tip)||!valid_digest(f[7])||sha256(f[8])!=f[7])
    return fail(Code::invalid,"invalid bounded mesh bootstrap");
  EngineIdentity id;EngineState state;if(!decode_checkpoint(f[8],id,state)||id!=offer.source)
    return fail(Code::corrupt,"bootstrap checkpoint context mismatch");
  offer.id=f[1];offer.subscription_id=f[2];offer.checkpoint_sha256=f[7];offer.checkpoint=std::move(f[8]);return {};
}

struct Service::Impl {
  ev::ICommitStore& store;Options options;lc::Service lifecycle;
  Impl(ev::ICommitStore& value,Options bounds):store(value),options(bounds),lifecycle(value){}
  Result current(const Session& session,std::uint64_t now,ev::CommitBatch* batch=nullptr) const {
    if(store.info().operation_epoch)return fail(Code::unsupported,"built-in mesh requires retained receipt epoch zero");
    if(now<store.info().admitted_unix_s)return fail(Code::expired,"host clock is behind durable mesh authority time");
    lc::Snapshot snapshot;if(auto result=lifecycle.snapshot(snapshot);!result)return result;
    if(snapshot.quarantined)return fail(Code::unauthorized,"restored mesh store remains quarantined");
    lc::Peer peer;if(auto result=lifecycle.peer(session.principal,peer);!result)return result.code==Code::not_found?fail(Code::unauthorized,"peer is not enrolled"):result;
    if(peer.phase!=lc::PeerPhase::active||peer.generation!=session.credential_generation||
       peer.certificate_sha256!=session.certificate_sha256||peer.expires_unix_s<=now||session.certificate_expiry_unix_s<=now)
      return fail(Code::unauthorized,"mesh peer revoked, rotated or expired");
    if(batch){
      ev::StoredRecord record_;if(auto result=store.latest("lifecycle.peer",sha256(peer.principal),record_);!result)return result;
      guard(*batch,"lifecycle.peer",sha256(peer.principal),record_);
      batch->authorizations.push_back({peer.principal,"mesh/2",peer.generation,std::min(peer.expires_unix_s,session.certificate_expiry_unix_s)});
      ev::StoredRecord active;if(auto result=store.latest("lifecycle.active","state",active);!result)return result;
      guard(*batch,"lifecycle.active","state",active);
    }
    return {};
  }
  Result grant(std::string_view principal,const lc::SourceRef& source,Grant& out,ev::StoredRecord* stored=nullptr) const {
    ev::StoredRecord record_;if(auto result=store.latest("mesh.grant",grant_key(principal,source),record_);!result)return result.code==Code::not_found?fail(Code::unauthorized,"peer has no exact source grant"):result;
    if(record_.record.tombstone)return fail(Code::unauthorized,"mesh grant is retired");
    if(auto result=decode_grant(record_.record.payload,out);!result)return result;
    if(out.principal!=principal||scope(out.source)!=source)return fail(Code::corrupt,"mesh grant key mismatch");
    if(stored)*stored=std::move(record_);
    return {};
  }
  Result authorized(const Session& session,const lc::SourceRef& source,bool sending,std::uint64_t now,
                    Grant& out,ev::CommitBatch* batch=nullptr) const {
    if(auto result=current(session,now,batch);!result)return result;
    ev::StoredRecord record_;if(auto result=grant(session.principal,source,out,&record_);!result)return result;
    if(sending?!out.send:!out.receive)return fail(Code::unauthorized,"peer has no exact source direction grant");
    if(batch)guard(*batch,"mesh.grant",grant_key(session.principal,source),record_);
    lc::Snapshot snapshot;if(auto result=lifecycle.snapshot(snapshot);!result)return result;
    ev::StoredRecord binding;if(auto result=store.latest("mesh.grant.binding",grant_key(session.principal,source),binding);!result)return result;
    if(binding.record.tombstone||binding.record.payload!=pack({"mesh-grant-binding/2",snapshot.incarnation,snapshot.recovery_policy_sha256}))
      return fail(Code::unauthorized,"restored mesh scope requires explicit current-incarnation authorization");
    if(batch)guard(*batch,"mesh.grant.binding",grant_key(session.principal,source),binding);
    if(!sending){
      ev::StoredRecord owner;if(auto result=store.latest("mesh.receive.owner",sha256(source.uid),owner);!result)return result;
      std::vector<std::string> f;std::uint64_t epoch=0;
      if(!unpack(owner.record.payload,f,4,32768)||f.size()!=4||f[0]!="mesh-owner/2"||!integer(f[2],epoch))
        return fail(Code::corrupt,"invalid remote source ownership fence");
      if(f[1]!=session.principal||epoch!=source.epoch||f[3]!=encode_identity(out.source))
        return fail(Code::unauthorized,"remote source epoch or immutable ownership changed");
      if(batch)guard(*batch,"mesh.receive.owner",sha256(source.uid),owner);
    }
    return {};
  }
  Result retention_candidate(const ReceiverRetentionRequest& request,ev::CommitBatch& batch,ReceiverRetentionPlan& plan) const {
    if(!valid_retention_request(request))return fail(Code::invalid,"retention needs complete pages of at most 32 observations and at most eight artifacts/checkpoints each");
    if(store.info().operation_epoch)return fail(Code::unsupported,"receiver retention requires retained operation receipts");
    lc::Snapshot snapshot;if(auto result=lifecycle.snapshot(snapshot);!result)return result;
    if(snapshot.quarantined)return fail(Code::unauthorized,"reauthorize a restored receiver before retention");
    Grant granted;if(auto result=grant(request.principal,request.source,granted);!result)return result;
    lc::SourceAuthority owner;if(auto result=lifecycle.source_authority(request.source.uid,owner);!result)return result;
    if(owner.kind!=lc::SourceAuthorityKind::relay||owner.owner!=request.principal||owner.clock_domain!=granted.source.clock_domain)
      return fail(Code::unauthorized,"retention scope is not owned by this authenticated relay");
    batch={};batch.operation_id=operation("retention-candidate",encode_retention_request(request));
    batch.request_digest=sha256(encode_retention_request(request));batch.commit_class=ev::CommitClass::maintenance;
    std::set<Digest> objects,receipts;std::set<std::string> removed;
    const auto remove=[&](const ev::StoredRecord& stored)->Result {
      if(stored.record.tombstone)return fail(Code::history_expired,"receiver history was already explicitly expired");
      if(!removed.insert(pack({stored.record.kind,stored.record.key})).second)return {};
      guard(batch,stored.record.kind,stored.record.key,stored);
      auto record_=stored.record;record_.references.clear();record_.tombstone=true;batch.records.push_back(std::move(record_));return {};
    };
    const auto cursor_protects=[&](std::string_view subscription,std::string_view digest)->Result {
      ev::StoredRecord cursor;bool exists=false;
      if(auto result=read(store,"mesh.receive",receive_key(request.principal,subscription,request.source),cursor,exists);!result)return result;
      if(exists&&std::find(cursor.record.references.begin(),cursor.record.references.end(),digest)!=cursor.record.references.end())
        return fail(Code::busy,"latest receive cursor protects this object; advance delivery or install a newer bootstrap first");
      return {};
    };
    const auto remove_receipt=[&](const Digest& digest,bool complete_interval,std::uint64_t required_sequence)->Result {
      Bytes receipt;std::vector<std::string> fields;std::uint64_t credential=0;Delivery page;
      if(auto result=store.get_blob(digest,receipt);!result)return result;
      Options bounds;bounds.maximum_page_observations=32;bounds.maximum_page_bytes=65536;
      if(sha256(receipt)!=digest||!unpack(receipt,fields,5,131072)||fields.size()!=5||fields[0]!="mesh-relay-receipt/2"||
         fields[1]!=request.principal||!integer(fields[2],credential)||!credential||!valid_digest(fields[3])||
         !decode_delivery(fields[4],page,bounds)||page.source!=granted.source)
        return fail(Code::corrupt,"receiver retention page provenance differs from its scope");
      if(required_sequence&&(required_sequence<=page.from.sequence||required_sequence>page.to.sequence))
        return fail(Code::corrupt,"receiver observation is absent from its relay receipt");
      if(!receipts.insert(digest).second)return {};
      for(const auto& delivered:page.observations){
        ev::StoredRecord known;std::vector<std::string> claim;
        if(auto result=store.latest("mesh.observation",received_observation_key(request.principal,request.source,delivered.claim.sequence),known);!result)return result;
        const bool selected=delivered.claim.sequence>=request.first_sequence&&delivered.claim.sequence<=request.last_sequence;
        if(complete_interval&&!selected&&!known.record.tombstone)
          return fail(Code::invalid,"retention must select every still-retained observation in an original delivery page");
        if((complete_interval&&selected&&known.record.tombstone)||!unpack(known.record.payload,claim,4,4096)||claim.size()!=4||claim[0]!="mesh-observation/2"||
           claim[1]!=delivered.claim.payload_digest||claim[2]!=delivered.claim.receipt_digest)
          return fail(Code::corrupt,"receiver page no longer matches its retained observation identities");
      }
      if(auto result=cursor_protects(page.subscription_id,digest);!result)return result;
      const auto op=operation("receive",pack({request.principal,fields[4]}));ev::StoredRecord witness;
      if(auto result=store.latest("mesh.delivery.receipt",op,witness);!result)return result;
      if(witness.record.tombstone)return fail(Code::history_expired,"receiver delivery receipt has already expired");
      if(witness.record.payload!=pack({"mesh-delivery-receipt/2",encode_cursor(page.to)})||witness.record.references!=std::vector<Digest>{digest})
        return fail(Code::corrupt,"receiver page has no exact original delivery witness");
      ev::CommitReceipt committed;if(auto result=store.lookup_operation(op,committed);!result)return result;
      if(committed.request_digest!=sha256(pack({request.principal,fields[4]})))return fail(Code::corrupt,"receiver page operation identity differs");
      if(auto result=remove(witness);!result)return result;
      objects.insert(digest);return {};
    };
    if(request.first_sequence)for(auto sequence=request.first_sequence;;++sequence){
      ev::StoredRecord observed;if(auto result=store.latest("mesh.observation",received_observation_key(request.principal,request.source,sequence),observed);!result)return result;
      if(observed.record.tombstone)return fail(Code::history_expired,"requested observation has already expired");
      std::vector<std::string> f;
      if(!unpack(observed.record.payload,f,4,4096)||f.size()!=4||f[0]!="mesh-observation/2"||
         !valid_digest(f[1])||!valid_digest(f[2])||!valid_digest(f[3])||observed.record.references!=std::vector<Digest>{f[1],f[3]})
        return fail(Code::corrupt,"invalid receiver observation retention witness");
      Bytes payload;if(auto result=store.get_blob(f[1],payload);!result)return result;
      lc::SourceRef decoded;std::uint64_t ordinal=0;Observation observation;
      if(sha256(payload)!=f[1]||!lc::decode_source_observation(payload,decoded,ordinal,observation)||decoded!=request.source||ordinal!=sequence)
        return fail(Code::corrupt,"receiver retention observation differs from its identity");
      ev::StoredRecord projected;bool exists=false;
      if(auto result=read(store,"mesh.projection",sha256(pack({encode_scope(request.source),observation.metric})),projected,exists);!result)return result;
      if(exists&&std::find(projected.record.references.begin(),projected.record.references.end(),f[1])!=projected.record.references.end())
        return fail(Code::busy,"latest relay projection still protects the requested observation");
      if(auto result=remove(observed);!result)return result;
      objects.insert(f[1]);
      if(auto result=remove_receipt(f[3],true,sequence);!result)return result;
      if(sequence==request.last_sequence)break;
    }
    for(const auto& digest:request.delivery_receipts)if(auto result=remove_receipt(digest,false,0);!result)return result;
    for(const auto& digest:request.artifacts){
      ev::StoredRecord artifact;if(auto result=store.latest("mesh.artifact",grant_key(request.principal,request.source)+":"+digest,artifact);!result)return result;
      if(artifact.record.tombstone)return fail(Code::history_expired,"receiver artifact has already expired");
      std::vector<std::string> f;std::uint64_t credential=0;
      if(!unpack(artifact.record.payload,f,4,4096)||f.size()!=4||f[0]!="mesh-artifact/2"||f[1]!=request.principal||
         !integer(f[2],credential)||!credential||f[3]!=digest||artifact.record.references!=std::vector<Digest>{digest})
        return fail(Code::corrupt,"artifact does not have an exact receiver ownership witness");
      if(auto result=remove(artifact);!result)return result;
      objects.insert(digest);
    }
    for(const auto& id:request.bootstrap_ids){
      const auto op=operation("bootstrap-install",pack({request.principal,id}));ev::StoredRecord installed;
      if(auto result=store.latest("mesh.bootstrap.installed",op,installed);!result)return result;
      if(installed.record.tombstone)return fail(Code::history_expired,"receiver bootstrap has already expired");
      std::vector<std::string> f;EngineIdentity context;std::uint64_t generation=0,lease=0;lc::Cursor tip;
      if(!unpack(installed.record.payload,f,8,32768)||f.size()!=8||f[0]!="mesh-bootstrap-offer/2"||f[1]!=id||
         !valid_token(f[2],256)||!integer(f[3],generation)||!generation||!integer(f[4],lease)||!decode_identity(f[5],context)||
         context!=granted.source||!decode_cursor(f[6],tip)||!valid_digest(f[7])||installed.record.references!=std::vector<Digest>{f[7]})
        return fail(Code::corrupt,"bootstrap installation does not match receiver retention scope");
      if(auto result=cursor_protects(f[2],f[7]);!result)return result;
      if(auto result=remove(installed);!result)return result;
      objects.insert(f[7]);
    }
    if(objects.size()>64)return fail(Code::capacity,"receiver retention page exceeds the 64-object expiry bound");
    plan={};plan.request=request;const auto info=store.info();plan.store_id=info.store_id;plan.reviewed_root=info.root_digest;
    plan.reviewed_generation=info.generation+1;
    if(!plan.reviewed_generation)return fail(Code::capacity,"receiver retention generation exhausted");
    for(const auto& digest:objects){
      ev::BlobInfo object;if(auto result=store.blob_info(digest,object);!result)return result;
      if(object.expired_generation)return fail(Code::history_expired,"requested receiver object has already expired");
      if(!object.present)return fail(Code::corrupt,"receiver retention object is missing");
      Bytes bytes;if(auto result=store.get_blob(digest,bytes);!result)return result;
      if(bytes.size()!=object.bytes||sha256(bytes)!=digest)return fail(Code::corrupt,"receiver retention object cannot be verified");
      if(object.bytes>std::numeric_limits<std::uint64_t>::max()-plan.expected_bytes)return fail(Code::capacity,"receiver retention byte count overflow");
      plan.expected_bytes+=object.bytes;
    }
    plan.objects.assign(objects.begin(),objects.end());batch.expire_blobs=plan.objects;
    plan.candidate_sha256=ev::batch_digest(batch);return {};
  }
};
Service::Service(ev::ICommitStore& store,Options options):impl_(std::make_unique<Impl>(store,options)){}
Service::~Service()=default;
Result Service::authenticate(const AuthenticatedIdentity& identity,std::string_view principal,std::uint64_t now,Session& out) const {
  out={};if(!identity.verified||!valid_digest(identity.certificate_sha256)||!valid_token(principal,256)||identity.certificate_expiry_unix_s<=now)
    return fail(Code::unauthorized,"unverified mesh transport identity");
  lc::Peer peer;if(auto result=impl_->lifecycle.peer(principal,peer);!result)return result.code==Code::not_found?fail(Code::unauthorized,"peer is not enrolled"):result;
  Session session{std::string(principal),identity.certificate_sha256,peer.generation,identity.certificate_expiry_unix_s,
    std::min(peer.expires_unix_s,identity.certificate_expiry_unix_s)};
  if(auto result=impl_->current(session,now);!result)return result;
  out=std::move(session);return {};
}
Result Service::authorize(const Session& session,const lc::SourceRef& source,bool sending,std::uint64_t now,Grant* out) const {
  Grant grant_;const auto result=impl_->authorized(session,source,sending,now,grant_);if(result&&out)*out=std::move(grant_);return result;
}
Result Service::authorize_delivery(const Session& session,const Delivery& page,std::uint64_t now) const {
  Grant grant_;if(auto result=impl_->authorized(session,scope(page.source),true,now,grant_);!result)return result;
  lc::Subscription subscription;if(auto result=impl_->lifecycle.subscription(page.subscription_id,subscription);!result)return result;
  if(grant_.source!=page.source||subscription.principal!=session.principal||subscription.scope!=scope(page.source)||
      subscription.generation!=page.generation||subscription.phase!=lc::SubscriptionPhase::active||subscription.lease_until_unix_s<=now||
      !subscription.has_offer||subscription.offered!=page.to||subscription.acknowledged!=page.from)
    return fail(Code::unauthorized,"queued delivery authority, exact offer or subscription generation changed");
  return impl_->lifecycle.replay_status(subscription.scope,subscription.acknowledged.sequence);
}
Result Service::authorize_artifact(const Session& session,const lc::SourceRef& source,std::string_view digest,std::uint64_t now) const {
  Grant grant_;if(auto result=impl_->authorized(session,source,true,now,grant_);!result)return result;
  if(std::find(grant_.artifacts.begin(),grant_.artifacts.end(),digest)==grant_.artifacts.end())return fail(Code::unauthorized,"queued artifact is no longer granted");
  ev::BlobInfo blob;if(auto result=impl_->store.blob_info(digest,blob);!result)return result;
  return !blob.expired_generation&&blob.present?Result{}:fail(Code::history_expired,"queued artifact is no longer retained");
}
Result Service::authorize_bootstrap(const Session& session,const BootstrapOffer& offer,std::uint64_t now) const {
  Grant grant_;if(auto result=impl_->authorized(session,scope(offer.source),true,now,grant_);!result)return result;
  if(!grant_.bootstrap||grant_.source!=offer.source||offer.lease_until_unix_s<=now)return fail(Code::unauthorized,"queued bootstrap scope or lease changed");
  ev::StoredRecord stored;if(auto result=impl_->store.latest("mesh.bootstrap.offer",offer.id,stored);!result)return result;
  if(stored.record.tombstone||stored.record.payload!=offer_metadata(offer))return fail(Code::unauthorized,"bootstrap offer was released or changed");
  lc::Pin pin;if(auto result=impl_->lifecycle.pin(offer.id,pin);!result)return result;
  if(pin.owner!=session.principal||pin.generation!=1||pin.kind!=lc::PinKind::reader||pin.objects!=std::vector<Digest>{offer.checkpoint_sha256})
    return fail(Code::unauthorized,"bootstrap protection no longer matches its offer");
  lc::Subscription subscription;const auto loaded=impl_->lifecycle.subscription(offer.subscription_id,subscription);
  if(!loaded&&loaded.code!=Code::not_found)return loaded;
  if(offer.generation!=(loaded?subscription.generation:0)+1||(loaded&&(subscription.scope!=scope(offer.source)||subscription.principal!=session.principal)))
    return fail(Code::unauthorized,"bootstrap subscription generation changed before handoff");
  return {};
}
Result Service::bootstrap_accepted(const Session& session,std::string_view id,std::uint64_t generation,std::string_view checkpoint,const lc::Cursor& tip,std::uint64_t now) const {
  lc::Bootstrap installed;if(auto result=impl_->lifecycle.installed_bootstrap(id,installed);!result)return result;
  Grant grant_;if(auto result=impl_->authorized(session,installed.scope,true,now,grant_);!result)return result;
  EngineState state;if(auto result=decode_state(installed.snapshot,state);!result)return result;
  if(installed.generation<generation)return fail(Code::not_found,"requested bootstrap generation has not been accepted");
  if(!grant_.bootstrap||installed.principal!=session.principal||installed.generation!=generation||installed.tip!=tip||checkpoint_digest(grant_.source,state)!=checkpoint)
    return fail(Code::conflict,"bootstrap acceptance does not match durable installation identity");
  return {};
}
Result Service::grant(std::string_view principal,const lc::SourceRef& source,Grant& out) const {return impl_->grant(principal,source,out);}
Result Service::configure_grant(const Grant& requested,std::uint64_t expected,std::string operation_id){
  if(impl_->store.info().operation_epoch)return fail(Code::unsupported,"built-in mesh requires retained receipt epoch zero");
  if(!valid_token(requested.principal,256)||!valid_identity(requested.source)||expected==std::numeric_limits<std::uint64_t>::max()||
      requested.generation!=expected+1||requested.artifacts.size()>impl_->options.maximum_artifacts)
    return fail(Code::invalid,"invalid mesh grant identity or generation");
  Grant roundtrip;if(!decode_grant(encode_grant(requested),roundtrip))return fail(Code::invalid,"invalid artifact scope");
  lc::Snapshot snapshot;if(auto result=impl_->lifecycle.snapshot(snapshot);!result)return result;
  if(snapshot.quarantined)return fail(Code::unauthorized,"restored mesh scope cannot be granted before current policy reauthorization");
  const auto binding_payload=pack({"mesh-grant-binding/2",snapshot.incarnation,snapshot.recovery_policy_sha256});
  const auto key=grant_key(requested.principal,scope(requested.source));
  ev::CommitBatch batch;batch.operation_id=std::move(operation_id);batch.request_digest=sha256(encode_grant(requested));
  ev::CommitReceipt prior;auto lookup=impl_->store.lookup_operation(batch.operation_id,prior);
  if(lookup){
    if(prior.request_digest!=batch.request_digest)return fail(Code::conflict,"grant operation identity reused");
    ev::StoredRecord binding;if(auto result=impl_->store.latest("mesh.grant.binding",key,binding);!result)return result;
    return !binding.record.tombstone&&binding.record.payload==binding_payload?Result{}:fail(Code::expired,"old grant operation cannot authorize a restored incarnation; use a new operation and generation");
  }
  if(lookup.code!=Code::not_found)return lookup;
  ev::StoredRecord old;bool found=false;
  if(auto result=read(impl_->store,"mesh.grant",key,old,found);!result)return result;
  Grant previous;if(found){if(auto result=decode_grant(old.record.payload,previous);!result)return result;}
  if((found?previous.generation:0)!=expected)return fail(Code::conflict,"stale mesh grant generation");
  if(found&&(previous.principal!=requested.principal||previous.source!=requested.source))return fail(Code::unauthorized,"mesh scope context is immutable");
  lc::Peer peer;if(auto result=impl_->lifecycle.peer(requested.principal,peer);!result)return result;
  if(requested.send){lc::Source source;if(auto result=impl_->lifecycle.source(scope(requested.source),source);!result)return result;
    if(source.identity!=requested.source)return fail(Code::conflict,"outbound scope differs from canonical source context");}
  if(requested.receive){
    if(auto result=impl_->lifecycle.stage_source_authority(batch,{requested.source.source_uid,requested.principal,
          requested.source.clock_domain,lc::SourceAuthorityKind::relay});!result)return result;
    lc::Source local;const auto local_result=impl_->lifecycle.source(scope(requested.source),local);
    if(local_result)return fail(Code::conflict,"remote receive cannot claim a locally owned source");
    if(local_result.code!=Code::not_found)return local_result;
    ev::StoredRecord owner;bool owned=false;const auto owner_key=sha256(requested.source.source_uid);
    if(auto result=read(impl_->store,"mesh.receive.owner",owner_key,owner,owned);!result)return result;
    if(owned){
      std::vector<std::string> f;std::uint64_t epoch=0;EngineIdentity identity;
      if(!unpack(owner.record.payload,f,4,32768)||f.size()!=4||f[0]!="mesh-owner/2"||!integer(f[2],epoch)||!decode_identity(f[3],identity))
        return fail(Code::corrupt,"invalid remote source ownership record");
      if(f[1]!=requested.principal||identity.clock_domain!=requested.source.clock_domain||requested.source.epoch<epoch||
         (requested.source.epoch==epoch&&identity!=requested.source)||
         (requested.source.epoch>epoch&&(epoch==std::numeric_limits<std::uint64_t>::max()||requested.source.epoch!=epoch+1)))
        return fail(Code::unauthorized,"remote source owner, clock or epoch fence mismatch");
    }
    guard(batch,"mesh.receive.owner",owner_key,owner);
    record(batch,"mesh.receive.owner",owner_key,pack({"mesh-owner/2",requested.principal,number(requested.source.epoch),encode_identity(requested.source)}));
  }
  guard(batch,"mesh.grant",key,old);record(batch,"mesh.grant",key,encode_grant(requested));
  ev::StoredRecord binding;bool bound=false;if(auto result=read(impl_->store,"mesh.grant.binding",key,binding,bound);!result)return result;
  guard(batch,"mesh.grant.binding",key,binding);record(batch,"mesh.grant.binding",key,binding_payload);
  ev::StoredRecord active;if(auto result=impl_->store.latest("lifecycle.active","state",active);!result)return result;
  guard(batch,"lifecycle.active","state",active);
  ev::CommitReceipt committed;return impl_->store.commit(batch,committed);
}
Result Service::subscribe(const Session& session,const lc::Subscription& requested,std::uint64_t now){
  ev::CommitBatch authority;Grant grant_;if(auto result=impl_->authorized(session,requested.scope,true,now,grant_,&authority);!result)return result;
  if(requested.principal!=session.principal)return fail(Code::unauthorized,"subscription belongs to another principal");
  lc::Subscription existing;auto found=impl_->lifecycle.subscription(requested.id,existing);
  if(found&&existing.generation==requested.generation){
    if(existing.principal!=session.principal||existing.scope!=requested.scope||existing.phase!=lc::SubscriptionPhase::active||existing.lease_until_unix_s<=now)
      return fail(Code::expired,"subscription cannot resume");
    return impl_->lifecycle.replay_status(requested.scope,existing.acknowledged.sequence);
  }
  if(!found&&found.code!=Code::not_found)return found;
  AuthorizedStore store(impl_->store,authority);lc::Service lifecycle(store);
  return lifecycle.open_subscription(requested,now,operation("subscribe",pack({requested.id,number(requested.generation),number(requested.lease_until_unix_s)})));
}
Result Service::next(const Session& session,std::string_view id,std::uint64_t generation,std::uint64_t now,Delivery& out,TransferCost& cost){
  out={};cost={};lc::Subscription subscription;if(auto result=impl_->lifecycle.subscription(id,subscription);!result)return result;
  ev::CommitBatch authority;Grant grant_;if(auto result=impl_->authorized(session,subscription.scope,true,now,grant_,&authority);!result)return result;
  if(subscription.principal!=session.principal||subscription.generation!=generation)return fail(Code::unauthorized,"subscription session or generation mismatch");
  if(subscription.phase!=lc::SubscriptionPhase::active||subscription.lease_until_unix_s<=now)return fail(Code::expired,"subscription is not active");
  if(auto result=impl_->lifecycle.replay_status(subscription.scope,subscription.acknowledged.sequence);!result)return result;
  lc::Source source;if(auto result=impl_->lifecycle.source(subscription.scope,source);!result)return result;
  if(source.identity!=grant_.source)return fail(Code::conflict,"source context differs from authorized scope");
  out={std::string(id),generation,source.identity,subscription.acknowledged,subscription.acknowledged,{}};
  const auto terminal=subscription.has_offer?subscription.offered.sequence:source.last_sequence;
  for(auto sequence=subscription.acknowledged.sequence;sequence<terminal&&out.observations.size()<impl_->options.maximum_page_observations;){
    ++sequence;DeliveryItem item;if(auto result=impl_->store.lookup_observation(source.id.uid,source.id.epoch,sequence,item.claim);!result)return result;
    ev::BlobInfo metadata;if(auto result=impl_->store.blob_info(item.claim.payload_digest,metadata);!result)return result;
    cost.work+=2;
    // Refuse oversized work from indexed metadata before reading/hash work.
    // A conservative 20-byte allowance covers the payload length prefix.
    out.observations.push_back(item);out.to={sequence,item.claim.payload_digest};
    const auto fixed=encode_delivery(out).size();out.observations.pop_back();
    out.to=out.observations.empty()?out.from:lc::Cursor{out.observations.back().claim.sequence,out.observations.back().claim.payload_digest};
    if(fixed>impl_->options.maximum_page_bytes||metadata.bytes>impl_->options.maximum_page_bytes-fixed||
        impl_->options.maximum_page_bytes-fixed-metadata.bytes<20)break;
    if(auto result=impl_->store.get_blob(item.claim.payload_digest,item.payload);!result)return result;
    cost.work+=1+item.payload.size()/4096;cost.bytes+=item.payload.size();
    out.observations.push_back(std::move(item));out.to={sequence,out.observations.back().claim.payload_digest};
    if(encode_delivery(out).size()>impl_->options.maximum_page_bytes){
      out.observations.pop_back();out.to=out.observations.empty()?out.from:lc::Cursor{out.observations.back().claim.sequence,out.observations.back().claim.payload_digest};break;
    }
  }
  if(out.observations.empty())return terminal>out.from.sequence?fail(Code::capacity,"one observation exceeds page budget"):fail(Code::completed,"caught up to canonical source tip");
  if(subscription.has_offer&&out.to!=subscription.offered)return fail(Code::capacity,"configured page bounds cannot reproduce durable outstanding offer");
  Delivery checked;if(auto result=decode_delivery(encode_delivery(out),checked,impl_->options);!result)return result;
  AuthorizedStore store(impl_->store,authority);lc::Service lifecycle(store);
  if(auto result=lifecycle.offer(id,generation,out.to,now,operation("offer",encode_delivery(out)));!result.committed())return result;
  cost.bytes+=encode_delivery(out).size();++cost.work;return {};
}
Result Service::acknowledge(const Session& session,const Delivery& page,std::uint64_t now){
  ev::CommitBatch authority;Grant grant_;if(auto result=impl_->authorized(session,scope(page.source),true,now,grant_,&authority);!result)return result;
  lc::Subscription sub;if(auto result=impl_->lifecycle.subscription(page.subscription_id,sub);!result)return result;
  if(sub.principal!=session.principal||sub.scope!=scope(page.source)||grant_.source!=page.source)return fail(Code::unauthorized,"ACK scope mismatch");
  AuthorizedStore store(impl_->store,authority);lc::Service lifecycle(store);
  return lifecycle.acknowledge(page.subscription_id,page.generation,page.to,now,operation("ack",pack({page.subscription_id,number(page.generation),encode_cursor(page.to)})));
}
Result Service::received(std::string_view principal,std::string_view id,const lc::SourceRef& source,ReceiveReceipt& out) const {
  out={};ev::StoredRecord value;const auto result=impl_->store.latest("mesh.receive",receive_key(principal,id,source),value);
  if(!result)return result;
  return decode_receive(value,out);
}
Result Service::received_observation(std::string_view principal,const lc::SourceRef& source,std::uint64_t sequence,DeliveryItem& out) const {
  out={};ev::StoredRecord stored;if(auto result=impl_->store.latest("mesh.observation",received_observation_key(principal,source,sequence),stored);!result)return result;
  if(stored.record.tombstone)return fail(Code::history_expired,"receiver observation was explicitly expired");
  std::vector<std::string> fields;
  if(!unpack(stored.record.payload,fields,4,4096)||fields.size()!=4||fields[0]!="mesh-observation/2"||
      !valid_digest(fields[1])||!valid_digest(fields[2])||!valid_digest(fields[3]))return fail(Code::corrupt,"invalid relay observation record");
  out.claim={source.uid,source.epoch,sequence,fields[1],fields[2]};
  if(auto result=impl_->store.get_blob(out.claim.payload_digest,out.payload);!result)return result;
  lc::SourceRef decoded;std::uint64_t ordinal=0;Observation observation;
  if(!lc::decode_source_observation(out.payload,decoded,ordinal,observation)||decoded!=source||ordinal!=sequence||sha256(out.payload)!=out.claim.payload_digest)
    return fail(Code::corrupt,"relay observation content mismatch");
  return {};
}
Result Service::receive(const Session& session,const Delivery& input,std::uint64_t now,ReceiveReceipt& out){
  out={};const auto bytes=encode_delivery(input);Delivery page;if(auto result=decode_delivery(bytes,page,impl_->options);!result)return result;
  ev::CommitBatch batch;batch.operation_id=operation("receive",pack({session.principal,bytes}));batch.request_digest=sha256(pack({session.principal,bytes}));
  Grant grant_;if(auto result=impl_->authorized(session,scope(page.source),false,now,grant_,&batch);!result)return result;
  if(page.source!=grant_.source)return fail(Code::unauthorized,"received page context differs from the authorized model and clock");
  ev::CommitReceipt committed;const auto prior=impl_->store.lookup_operation(batch.operation_id,committed);
  if(prior){
    ev::StoredRecord witness;if(auto result=impl_->store.latest("mesh.delivery.receipt",batch.operation_id,witness);!result)return result;
    if(witness.record.tombstone)return fail(Code::history_expired,"received page was explicitly expired; its operation identity remains retained");
    for(const auto& item:page.observations){
      ev::StoredRecord observed;if(auto result=impl_->store.latest("mesh.observation",received_observation_key(session.principal,scope(page.source),item.claim.sequence),observed);!result)return result;
      if(observed.record.tombstone)return fail(Code::history_expired,"received page overlaps explicitly expired observation history");
    }
    if(witness.record.references.size()!=1)return fail(Code::corrupt,"relay receipt witness is missing");
    Bytes receipt;if(auto result=impl_->store.get_blob(witness.record.references.front(),receipt);!result)return result;
    out={page.to,page.generation,committed.generation,witness.record.references.front(),true};return {};
  }
  if(prior.code!=Code::not_found)return prior;
  const auto key=receive_key(session.principal,page.subscription_id,scope(page.source));ev::StoredRecord previous;bool found=false;
  if(auto result=read(impl_->store,"mesh.receive",key,previous,found);!result)return result;
  ReceiveReceipt old;if(found){if(auto result=decode_receive(previous,old);!result)return result;}
  if(page.from!=old.cursor||(found&&(page.generation<old.generation||page.generation>old.generation+1)))
    return fail(Code::conflict,"received page does not continue durable cursor or subscription generation");
  const auto receipt_bytes=pack({"mesh-relay-receipt/2",session.principal,number(session.credential_generation),session.certificate_sha256,bytes});
  const auto receipt_digest=sha256(receipt_bytes);batch.blobs.push_back({receipt_digest,receipt_bytes,{}});
  for(const auto& item:page.observations){
    batch.blobs.push_back({item.claim.payload_digest,item.payload,{}});
    const auto observation_key=received_observation_key(session.principal,scope(page.source),item.claim.sequence);
    ev::StoredRecord previous_observation;bool exists=false;
    if(auto result=read(impl_->store,"mesh.observation",observation_key,previous_observation,exists);!result)return result;
    guard(batch,"mesh.observation",observation_key,previous_observation);
    if(exists){
      DeliveryItem original;if(auto result=received_observation(session.principal,scope(page.source),item.claim.sequence,original);!result)return result;
      if(original.payload!=item.payload||original.claim.receipt_digest!=item.claim.receipt_digest)return fail(Code::conflict,"relay sequence has different original evidence");
    }else record(batch,"mesh.observation",observation_key,pack({"mesh-observation/2",item.claim.payload_digest,item.claim.receipt_digest,receipt_digest}),{item.claim.payload_digest,receipt_digest});
    lc::SourceRef source;std::uint64_t sequence=0;Observation observation;
    if(auto result=lc::decode_source_observation(item.payload,source,sequence,observation);!result)return result;
    const auto projection_key=sha256(pack({encode_scope(source),observation.metric}));
    auto existing=std::find_if(batch.records.begin(),batch.records.end(),[&](const auto& rec){return rec.kind=="mesh.projection"&&rec.key==projection_key;});
    if(existing!=batch.records.end()){existing->payload=item.payload;existing->references={item.claim.payload_digest};}
    else {
      ev::StoredRecord projected;bool present=false;if(auto result=read(impl_->store,"mesh.projection",projection_key,projected,present);!result)return result;
      if(present&&!projected.record.tombstone){lc::SourceRef old_source;std::uint64_t old_sequence=0;Observation old_observation;
        if(!lc::decode_source_observation(projected.record.payload,old_source,old_sequence,old_observation)||old_source!=source)
          return fail(Code::corrupt,"invalid relay projection context");
        if(old_sequence>=item.claim.sequence)continue;
      }
      guard(batch,"mesh.projection",projection_key,projected);record(batch,"mesh.projection",projection_key,item.payload,{item.claim.payload_digest});
    }
  }
  guard(batch,"mesh.receive",key,previous);
  record(batch,"mesh.receive",key,pack({"mesh-receive/2",number(page.generation),encode_cursor(page.to),receipt_digest,encode_identity(page.source),"delivery"}),{receipt_digest,page.to.payload_sha256});
  record(batch,"mesh.delivery.receipt",batch.operation_id,pack({"mesh-delivery-receipt/2",encode_cursor(page.to)}),{receipt_digest});
  const auto result=impl_->store.commit(batch,committed);
  if(result.committed())out={page.to,page.generation,committed.generation,receipt_digest,committed.duplicate};
  return result;
}
Result Service::artifact_chunk(const Session& session,const lc::SourceRef& source,std::string_view digest,std::uint64_t offset,
                              std::size_t maximum,std::uint64_t now,ArtifactChunk& out,TransferCost& cost){
  out={};cost={};Grant grant_;if(auto result=impl_->authorized(session,source,true,now,grant_);!result)return result;
  if(!maximum||maximum>impl_->options.maximum_page_bytes||std::find(grant_.artifacts.begin(),grant_.artifacts.end(),digest)==grant_.artifacts.end())
    return fail(Code::unauthorized,"artifact is outside the explicit source grant");
  Bytes bytes;if(auto result=impl_->store.get_blob(digest,bytes);!result)return result;
  if(bytes.size()>impl_->options.maximum_artifact_bytes||offset>bytes.size())return fail(Code::capacity,"artifact size or offset exceeds profile");
  if(sha256(bytes)!=digest)return fail(Code::corrupt,"artifact content digest mismatch");
  cost={bytes.size(),2+bytes.size()/4096};out={std::string(digest),offset,bytes.size(),bytes.substr(static_cast<std::size_t>(offset),maximum)};return {};
}
Result Service::receive_artifact(const Session& session,const lc::SourceRef& source,std::string_view digest,Bytes bytes,std::uint64_t now,ReceiveReceipt& out){
  out={};if(!valid_digest(digest)||bytes.size()>impl_->options.maximum_artifact_bytes||sha256(bytes)!=digest)return fail(Code::corrupt,"artifact bounds or digest mismatch");
  ev::CommitBatch batch;batch.operation_id=operation("artifact",pack({session.principal,encode_scope(source),std::string(digest)}));batch.request_digest=sha256(bytes);
  Grant grant_;if(auto result=impl_->authorized(session,source,false,now,grant_,&batch);!result)return result;
  if(std::find(grant_.artifacts.begin(),grant_.artifacts.end(),digest)==grant_.artifacts.end())return fail(Code::unauthorized,"artifact receive has no exact grant");
  ev::CommitReceipt previous;const auto found=impl_->store.lookup_operation(batch.operation_id,previous);
  if(found){
    if(previous.request_digest!=batch.request_digest)return fail(Code::conflict,"artifact operation changed");
    Bytes retained;if(auto result=impl_->store.get_blob(digest,retained);!result)return result;
    if(retained!=bytes)return fail(Code::corrupt,"retained artifact differs from exact retransmission");
    out.store_generation=previous.generation;out.receipt_sha256=digest;out.duplicate=true;return {};
  }
  if(found.code!=Code::not_found)return found;
  batch.blobs.push_back({std::string(digest),std::move(bytes),{}});
  record(batch,"mesh.artifact",grant_key(session.principal,source)+":"+std::string(digest),pack({"mesh-artifact/2",session.principal,number(session.credential_generation),std::string(digest)}),{std::string(digest)});
  ev::CommitReceipt receipt;const auto result=impl_->store.commit(batch,receipt);
  if(result.committed()){out.store_generation=receipt.generation;out.receipt_sha256=digest;out.duplicate=receipt.duplicate;}
  return result;
}
Result Service::offer_bootstrap(const Session& session,std::string id,std::uint64_t generation,const lc::SourceRef& source,
                               std::uint64_t lease,std::uint64_t now,BootstrapOffer& out){
  out={};ev::CommitBatch authority;Grant grant_;if(auto result=impl_->authorized(session,source,true,now,grant_,&authority);!result)return result;
  if(!grant_.bootstrap||!valid_token(id,256)||lease<=now||lease-now>30ULL*24ULL*60ULL*60ULL)return fail(Code::unauthorized,"bootstrap requires explicit scope and bounded lease");
  lc::Subscription sub;const auto existing=impl_->lifecycle.subscription(id,sub);
  if(!existing&&existing.code!=Code::not_found)return existing;
  if(generation!=(existing?sub.generation:0)+1||(existing&&(sub.principal!=session.principal||sub.scope!=source)))return fail(Code::conflict,"bootstrap must advance the authorized subscription generation");
  lc::Source current;if(auto result=impl_->lifecycle.source(source,current);!result)return result;
  Bytes checkpoint;if(auto result=impl_->store.get_blob(current.state_object_sha256,checkpoint);!result)return result;
  if(checkpoint.size()>impl_->options.maximum_artifact_bytes)return fail(Code::capacity,"bootstrap checkpoint exceeds declared profile");
  out={operation("bootstrap",pack({id,number(generation),current.state_object_sha256,number(lease)})),std::move(id),generation,lease,current.identity,
    {current.last_sequence,current.last_payload_sha256},current.state_object_sha256,std::move(checkpoint)};
  ev::CommitReceipt previous;const auto found=impl_->store.lookup_operation(operation("bootstrap-offer",encode_bootstrap(out)),previous);
  if(found){ev::StoredRecord retained;if(auto result=impl_->store.latest("mesh.bootstrap.offer",out.id,retained);!result)return result;
    return !retained.record.tombstone&&retained.record.payload==offer_metadata(out)?Result{}:fail(Code::expired,"bootstrap offer was released");}
  if(found.code!=Code::not_found)return found;
  lc::Pin pin{out.id,session.principal,lc::PinKind::reader,1,lease,{out.checkpoint_sha256}};
  AuthorizedStore guarded(impl_->store,authority);lc::Service guarded_lifecycle(guarded);
  const auto pinned=guarded_lifecycle.put_pin(pin,0,now,operation("bootstrap-pin",encode_bootstrap(out)));
  if(!pinned.committed())return pinned;
  ev::CommitBatch batch;batch.operation_id=operation("bootstrap-offer",encode_bootstrap(out));batch.request_digest=sha256(encode_bootstrap(out));
  if(auto result=impl_->authorized(session,source,true,now,grant_,&batch);!result)return result;
  record(batch,"mesh.bootstrap.offer",out.id,offer_metadata(out),{out.checkpoint_sha256});
  ev::CommitReceipt receipt;return impl_->store.commit(batch,receipt);
}
Result Service::install_bootstrap(const Session& session,const BootstrapOffer& input,std::uint64_t now,ReceiveReceipt& out){
  out={};BootstrapOffer offer;if(auto result=decode_bootstrap(encode_bootstrap(input),offer,impl_->options.maximum_artifact_bytes+65536);!result)return result;
  ev::CommitBatch batch;batch.operation_id=operation("bootstrap-install",pack({session.principal,offer.id}));batch.request_digest=sha256(encode_bootstrap(offer));
  Grant grant_;if(auto result=impl_->authorized(session,scope(offer.source),false,now,grant_,&batch);!result)return result;
  if(!grant_.bootstrap||grant_.source!=offer.source||offer.lease_until_unix_s<=now)return fail(Code::unauthorized,"bootstrap context or lease is not authorized");
  ev::CommitReceipt receipt;const auto prior=impl_->store.lookup_operation(batch.operation_id,receipt);
  if(prior){
    if(receipt.request_digest!=batch.request_digest)return fail(Code::conflict,"bootstrap operation identity was reused with different content");
    ev::StoredRecord installed;if(auto result=impl_->store.latest("mesh.bootstrap.installed",batch.operation_id,installed);!result)return result;
    if(installed.record.tombstone)return fail(Code::history_expired,"receiver bootstrap installation was explicitly expired");
    if(installed.record.payload!=offer_metadata(offer)||installed.record.references!=std::vector<Digest>{offer.checkpoint_sha256})
      return fail(Code::corrupt,"durable bootstrap installation differs from the requested retry");
    Bytes checkpoint;if(auto result=impl_->store.get_blob(offer.checkpoint_sha256,checkpoint);!result)return result;
    if(checkpoint!=offer.checkpoint)return fail(Code::corrupt,"durable bootstrap checkpoint bytes differ");
    out={offer.tip,offer.generation,receipt.generation,offer.checkpoint_sha256,true};return {};
  }
  if(prior.code!=Code::not_found)return prior;
  const auto key=receive_key(session.principal,offer.subscription_id,scope(offer.source));ev::StoredRecord old;bool found=false;
  if(auto result=read(impl_->store,"mesh.receive",key,old,found);!result)return result;
  ReceiveReceipt previous;if(found){if(auto result=decode_receive(old,previous);!result)return result;}
  if(offer.generation<=previous.generation||offer.tip.sequence<previous.cursor.sequence)return fail(Code::conflict,"bootstrap cannot regress installed continuity");
  batch.blobs.push_back({offer.checkpoint_sha256,offer.checkpoint,{}});
  guard(batch,"mesh.receive",key,old);
  record(batch,"mesh.receive",key,pack({"mesh-receive/2",number(offer.generation),encode_cursor(offer.tip),offer.checkpoint_sha256,encode_identity(offer.source),"bootstrap"}),{offer.checkpoint_sha256});
  ev::StoredRecord installation;bool exists=false;if(auto result=read(impl_->store,"mesh.bootstrap.installed",batch.operation_id,installation,exists);!result)return result;
  if(exists)return fail(Code::conflict,"bootstrap installation witness exists without its operation receipt");
  guard(batch,"mesh.bootstrap.installed",batch.operation_id,installation);
  record(batch,"mesh.bootstrap.installed",batch.operation_id,offer_metadata(offer),{offer.checkpoint_sha256});
  const auto result=impl_->store.commit(batch,receipt);if(result.committed())out={offer.tip,offer.generation,receipt.generation,offer.checkpoint_sha256,receipt.duplicate};return result;
}
Result Service::accept_bootstrap(const Session& session,const BootstrapOffer& offer,std::uint64_t now){
  ev::CommitBatch authority;Grant grant_;if(auto result=impl_->authorized(session,scope(offer.source),true,now,grant_,&authority);!result)return result;
  if(!grant_.bootstrap||grant_.source!=offer.source)return fail(Code::unauthorized,"bootstrap scope no longer authorized");
  ev::StoredRecord stored;if(auto result=impl_->store.latest("mesh.bootstrap.offer",offer.id,stored);!result)return result;
  if(stored.record.payload!=offer_metadata(offer))return fail(Code::conflict,"bootstrap acceptance differs from the pinned offer");
  EngineIdentity identity;EngineState state;if(auto result=decode_checkpoint(offer.checkpoint,identity,state);!result)return result;
  lc::Bootstrap bootstrap{offer.subscription_id,session.principal,offer.generation,offer.lease_until_unix_s,scope(offer.source),offer.tip,state_digest(state),encode_state(state)};
  AuthorizedStore store(impl_->store,authority);lc::Service lifecycle(store);
  const auto installed=lifecycle.accept_bootstrap(bootstrap,now,operation("bootstrap-accepted",offer.id));
  if(!installed.committed())return installed;
  return cancel_bootstrap(offer.id,operation("bootstrap-release",offer.id));
}
Result Service::cancel_bootstrap(std::string_view id,std::string operation_id){
  if(!valid_token(id,256))return fail(Code::invalid,"invalid bootstrap offer identifier");
  const auto request=sha256(pack({"mesh-bootstrap-release/2",std::string(id)}));
  ev::CommitReceipt previous;const auto found=impl_->store.lookup_operation(operation_id,previous);
  if(found)return previous.request_digest==request?Result{}:fail(Code::conflict,"bootstrap release operation changed");
  if(found.code!=Code::not_found)return found;
  ev::StoredRecord offered;bool offered_present=false;if(auto result=read(impl_->store,"mesh.bootstrap.offer",id,offered,offered_present);!result)return result;
  if(offered_present){
    std::vector<std::string> metadata;if(!unpack(offered.record.payload,metadata,8,32768)||metadata.size()!=8||metadata[0]!="mesh-bootstrap-offer/2"||metadata[1]!=id)
      return fail(Code::corrupt,"invalid bootstrap offer metadata");
  }else{
    lc::Pin pending;if(auto result=impl_->lifecycle.pin(id,pending);!result)return result;
    if(!id.starts_with("mesh2-bootstrap-")||pending.kind!=lc::PinKind::reader||pending.generation!=1)
      return fail(Code::invalid,"missing offer does not identify a bootstrap preparation pin");
  }
  const auto released=impl_->lifecycle.release_pin(id,1,operation_id+"-pin");if(!released.committed())return released;
  ev::CommitBatch batch;batch.operation_id=std::move(operation_id);batch.request_digest=request;
  batch.commit_class=ev::CommitClass::maintenance;
  if(offered_present){guard(batch,"mesh.bootstrap.offer",std::string(id),offered);batch.records.push_back({"mesh.bootstrap.offer",std::string(id),offered.record.payload,{},true});}
  else record(batch,"mesh.bootstrap.cancelled",std::string(id),pack({"mesh-bootstrap-abandoned/2",std::string(id)}));
  ev::CommitReceipt receipt;return impl_->store.commit(batch,receipt);
}

Result Service::receiver_retention(std::string_view id,ReceiverRetentionPlan& out) const {
  if(!valid_token(id,256))return fail(Code::invalid,"invalid receiver retention plan identifier");
  ev::StoredRecord stored;if(auto result=impl_->store.latest("mesh.retention.plan",sha256(id),stored);!result)return result;
  if(stored.record.tombstone)return fail(Code::corrupt,"receiver retention plan cannot be removed");
  if(auto result=decode_retention_plan(stored.record.payload,out);!result)return result;
  return out.request.id==id?Result{}:fail(Code::corrupt,"receiver retention plan key mismatch");
}
Result Service::review_receiver_retention(const ReceiverRetentionRequest& request,std::string id,ReceiverRetentionPlan& out){
  out={};if(!valid_retention_request(request))return fail(Code::invalid,"invalid bounded receiver retention request");
  const auto digest=sha256(pack({"mesh-receiver-retention-review/1",encode_retention_request(request)}));bool duplicate=false;
  if(auto result=retention_retry(impl_->store,id,digest,duplicate);!result)return result;
  if(duplicate)return receiver_retention(request.id,out);
  ev::StoredRecord previous;bool exists=false;const auto key=sha256(request.id);
  if(auto result=read(impl_->store,"mesh.retention.plan",key,previous,exists);!result)return result;
  if(exists)return fail(Code::conflict,"receiver retention plan identifier already exists");
  ev::CommitBatch candidate;ReceiverRetentionPlan plan;if(auto result=impl_->retention_candidate(request,candidate,plan);!result)return result;
  ev::CommitBatch batch;batch.operation_id=std::move(id);batch.request_digest=digest;batch.commit_class=ev::CommitClass::maintenance;
  batch.expected_generation=plan.reviewed_generation-1;guard(batch,"mesh.retention.plan",key,previous);
  record(batch,"mesh.retention.plan",key,encode_retention_plan(plan));ev::CommitReceipt receipt;
  const auto result=impl_->store.commit(batch,receipt);if(result.committed())out=std::move(plan);return result;
}
Result Service::activate_receiver_retention(std::string_view plan_id,std::string id){
  if(!valid_token(plan_id,256))return fail(Code::invalid,"invalid receiver retention plan identifier");
  const auto digest=sha256(pack({"mesh-receiver-retention-activate/1",std::string(plan_id)}));bool duplicate=false;
  if(auto result=retention_retry(impl_->store,id,digest,duplicate);!result||duplicate)return result;
  ReceiverRetentionPlan plan;if(auto result=receiver_retention(plan_id,plan);!result)return result;
  if(plan.phase!=lc::MaintenancePhase::reviewed)return fail(Code::conflict,"receiver retention is no longer in review");
  const auto info=impl_->store.info();
  if(info.store_id!=plan.store_id||info.generation!=plan.reviewed_generation)return fail(Code::conflict,"store changed after receiver retention review; create a new plan");
  ev::CommitBatch batch;ReceiverRetentionPlan rebuilt;
  if(auto result=impl_->retention_candidate(plan.request,batch,rebuilt);!result)return result;
  if(rebuilt.candidate_sha256!=plan.candidate_sha256||rebuilt.objects!=plan.objects||rebuilt.expected_bytes!=plan.expected_bytes)
    return fail(Code::conflict,"receiver retention candidates differ from the reviewed plan");
  ev::StoredRecord previous;const auto key=sha256(plan_id);if(auto result=impl_->store.latest("mesh.retention.plan",key,previous);!result)return result;
  plan.phase=lc::MaintenancePhase::activated;batch.operation_id=std::move(id);batch.request_digest=digest;
  batch.expected_generation=info.generation;guard(batch,"mesh.retention.plan",key,previous);
  record(batch,"mesh.retention.plan",key,encode_retention_plan(plan));ev::CommitReceipt receipt;
  return impl_->store.commit(batch,receipt);
}
Result Service::reclaim_receiver_retention(std::string_view plan_id,std::string id){
  if(!valid_token(plan_id,256))return fail(Code::invalid,"invalid receiver retention plan identifier");
  const auto digest=sha256(pack({"mesh-receiver-retention-reclaim/1",std::string(plan_id)}));bool duplicate=false;
  if(auto result=retention_retry(impl_->store,id,digest,duplicate);!result||duplicate)return result;
  ReceiverRetentionPlan plan;if(auto result=receiver_retention(plan_id,plan);!result)return result;
  if(plan.phase==lc::MaintenancePhase::reviewed)return fail(Code::conflict,"activate receiver retention before deleting bytes");
  if(plan.phase==lc::MaintenancePhase::completed)return fail(Code::completed,"receiver retention was already reclaimed");
  if(impl_->store.info().store_id!=plan.store_id)return fail(Code::conflict,"receiver retention store identity changed");
  for(const auto& digest_:plan.objects){
    ev::BlobInfo object;if(auto result=impl_->store.blob_info(digest_,object);!result)return result;
    if(!object.expired_generation)return fail(Code::corrupt,"receiver retention object lacks canonical expiry");
    // Physical deletion precedes its canonical accounting commit. A crash can
    // leave present=false while the quota still charges these bytes; replay
    // reclamation even then so the store reconciles the missing-file case.
    if(auto result=impl_->store.reclaim_expired_blob(digest_,impl_->store.generation());!result)return result;
  }
  ev::StoredRecord previous;const auto key=sha256(plan_id);if(auto result=impl_->store.latest("mesh.retention.plan",key,previous);!result)return result;
  plan.phase=lc::MaintenancePhase::completed;ev::CommitBatch batch;batch.operation_id=std::move(id);batch.request_digest=digest;
  batch.commit_class=ev::CommitClass::maintenance;guard(batch,"mesh.retention.plan",key,previous);
  record(batch,"mesh.retention.plan",key,encode_retention_plan(plan));ev::CommitReceipt receipt;return impl_->store.commit(batch,receipt);
}

struct FairScheduler::Impl {
  struct Pending {ScheduleKey key;TransferCost cost,deficit;};
  SchedulerOptions options;std::vector<Pending> pending;std::size_t cursor=0;
  std::map<std::string,TransferCost> principals,sources;
  explicit Impl(SchedulerOptions value):options(value){}
};
FairScheduler::FairScheduler(SchedulerOptions options):impl_(std::make_unique<Impl>(options)){}
FairScheduler::~FairScheduler()=default;
Result FairScheduler::enqueue(ScheduleKey key,TransferCost cost){
  if(!key.connection||!valid_token(key.principal,256)||key.source.size()>512||!cost.work||
     !impl_->options.byte_quantum||!impl_->options.work_quantum||cost.bytes>impl_->options.principal_byte_budget||cost.bytes>impl_->options.source_byte_budget||
     cost.work>impl_->options.principal_work_budget||cost.work>impl_->options.source_work_budget)
    return fail(Code::capacity,"work unit exceeds fair scheduling profile");
  if(impl_->pending.size()>=impl_->options.maximum_connections)return fail(Code::capacity,"mesh scheduling queue is full");
  if(std::any_of(impl_->pending.begin(),impl_->pending.end(),[&](const auto& pending){return pending.key.connection==key.connection;}))
    return fail(Code::busy,"connection already owns its one queued work unit");
  impl_->pending.push_back({std::move(key),cost,{}});return {};
}
bool FairScheduler::take(std::uint64_t& connection){
  connection=0;if(impl_->pending.empty())return false;
  // Churning principals never become an unbounded historical in-memory map.
  if(impl_->principals.size()>impl_->options.maximum_connections||impl_->sources.size()>impl_->options.maximum_connections){impl_->principals.clear();impl_->sources.clear();}
  // A bounded round reset is a scheduler turn, not a sleep or store poll.
  for(unsigned round=0;round<2;++round){
    for(std::size_t visited=0;visited<impl_->pending.size();++visited){
      if(impl_->cursor>=impl_->pending.size())impl_->cursor=0;
      auto& item=impl_->pending[impl_->cursor];
      item.deficit.bytes=std::min(impl_->options.principal_byte_budget,item.deficit.bytes+impl_->options.byte_quantum);
      item.deficit.work=std::min(std::numeric_limits<std::uint64_t>::max()-impl_->options.work_quantum,item.deficit.work)+impl_->options.work_quantum;
      auto& principal=impl_->principals[item.key.principal];auto& source=impl_->sources[item.key.source];
      if(item.cost.bytes<=item.deficit.bytes&&item.cost.work<=item.deficit.work&&
         item.cost.bytes<=impl_->options.principal_byte_budget-principal.bytes&&item.cost.bytes<=impl_->options.source_byte_budget-source.bytes&&
         item.cost.work<=impl_->options.principal_work_budget-principal.work&&item.cost.work<=impl_->options.source_work_budget-source.work){
        principal.bytes+=item.cost.bytes;principal.work+=item.cost.work;source.bytes+=item.cost.bytes;source.work+=item.cost.work;connection=item.key.connection;
        impl_->pending.erase(impl_->pending.begin()+static_cast<std::ptrdiff_t>(impl_->cursor));
        if(impl_->pending.empty()){impl_->principals.clear();impl_->sources.clear();}
        return true;
      }
      ++impl_->cursor;
    }
    impl_->principals.clear();impl_->sources.clear();
  }
  return false;
}
void FairScheduler::cancel(std::uint64_t connection){std::erase_if(impl_->pending,[&](const auto& item){return item.key.connection==connection;});}
std::size_t FairScheduler::queued()const noexcept{return impl_->pending.size();}
}
