#include "package_retention.hpp"
#include <algorithm>
#include <charconv>
#include <map>
#include <set>

namespace anaiero::deployment {
namespace {
namespace ev=evidence;
bool number(std::string_view text,std::uint64_t& value){const auto r=std::from_chars(text.data(),text.data()+text.size(),value);return !text.empty()&&r.ec==std::errc{}&&r.ptr==text.data()+text.size()&&std::to_string(value)==text;}
bool digests(const std::vector<Digest>& values,std::size_t bound){Digest previous;if(values.size()>bound)return false;for(const auto& value:values){if(!valid_digest(value)||(!previous.empty()&&value<=previous))return false;previous=value;}return true;}
Result corrupt(std::string message){return {Code::corrupt,std::move(message)};}
Bytes policy_wire(const PackagePolicy& policy){return pack({"anaiero.package-policy/1",std::to_string(policy.maximum_packages),std::to_string(policy.maximum_bytes),pack(policy.packages)});}
Result load_policy(ev::ICommitStore& store,PackagePolicy& policy,ev::StoredRecord& record){
  policy={};record={};const auto found=store.latest("operator.deployment-retention","policy",record);
  if(found.code==Code::not_found){
    // One-time bounded import of the older package records. Once written, the
    // active catalog avoids scanning the growing historical tombstone set.
    std::vector<ev::StoredRecord> page;bool more=false;
    if(auto r=store.scan_latest("operator.deployment-package",{},{},256,page,more);!r)return r;
    if(more)return {Code::capacity,"legacy package catalog exceeds the 256-package import bound"};
    for(const auto& item:page)if(!item.record.tombstone)policy.packages.push_back(item.record.key);
    return digests(policy.packages,256)?Result{}:corrupt("invalid retained package catalog");
  }
  if(!found)return found;
  std::vector<std::string> fields;
  if(record.record.tombstone||!unpack(record.record.payload,fields,4,32768)||fields.size()!=4||fields[0]!="anaiero.package-policy/1"||
     !number(fields[1],policy.maximum_packages)||!number(fields[2],policy.maximum_bytes)||!unpack(fields[3],policy.packages,256,32768)||
     policy.maximum_packages<2||policy.maximum_packages>256||policy.maximum_bytes<65536||policy.maximum_bytes>8ULL*1024ULL*1024ULL*1024ULL||
     !digests(policy.packages,256))return corrupt("invalid bounded package-retention policy");
  return {};
}
void stage_policy(const PackagePolicy& policy,const ev::StoredRecord& previous,ev::CommitBatch& batch){
  batch.guards.push_back({"operator.deployment-retention","policy",previous.revision,previous.digest});
  batch.records.push_back({"operator.deployment-retention","policy",policy_wire(policy),{},false});
}
Result package_references(ev::ICommitStore& store,std::string_view id,std::set<Digest>& references){
  for(const auto kind:{"operator.deployment-package","operator.deployment-witness"}){
    ev::StoredRecord record;const auto found=store.latest(kind,id,record);
    if(found.code==Code::not_found&&std::string_view(kind)=="operator.deployment-witness")continue;
    if(!found)return found;
    if(record.record.tombstone)return {Code::history_expired,"package witness has been explicitly retired"};
    references.insert(record.record.references.begin(),record.record.references.end());
  }
  return {};
}
Result binding_references(ev::ICommitStore& store,std::set<Digest>& refs){
  ev::StoredRecord binding;const auto found=store.latest("operator.deployment-binding","active",binding);
  if(found.code==Code::not_found)return {};
  if(!found)return found;
  if(!binding.record.tombstone)refs.insert(binding.record.references.begin(),binding.record.references.end());
  return {};
}
Result bytes_for(ev::ICommitStore& store,const std::set<Digest>& references,std::uint64_t& bytes,const std::map<Digest,std::uint64_t>& added={}){
  bytes=0;
  for(const auto& id:references){const auto extra=added.find(id);
    if(extra!=added.end()){bytes+=extra->second;continue;}
    ev::BlobInfo blob;if(auto r=store.blob_info(id,blob);!r)return r;
    if(blob.expired_generation||!blob.present)return corrupt("active package witness references expired or absent bytes");
    bytes+=blob.bytes;
  }
  return {};
}
Bytes retention_body(const PackageRetentionPlan& plan){return pack({"anaiero.package-retention/1",plan.operation_id,plan.store_id,std::to_string(plan.generation),plan.root_sha256,pack(plan.packages),pack(plan.objects),std::to_string(plan.bytes)});}
Result validate(const PackageRetentionPlan& plan){
  if(!valid_token(plan.operation_id,128)||!valid_digest(plan.store_id)||!valid_digest(plan.root_sha256)||!digests(plan.packages,8)||plan.packages.empty()||
     !digests(plan.objects,32)||!valid_digest(plan.plan_sha256)||sha256(retention_body(plan))!=plan.plan_sha256)return corrupt("invalid package-retention plan");
  return {};
}
}
Result package_usage(ev::ICommitStore& store,PackageUsage& usage){
  usage={};ev::StoredRecord record;if(auto r=load_policy(store,usage.policy,record);!r)return r;
  std::set<Digest> refs;for(const auto& id:usage.policy.packages)if(auto r=package_references(store,id,refs);!r)return r;
  if(auto r=binding_references(store,refs);!r)return r;
  return bytes_for(store,refs,usage.retained_bytes);
}
Result stage_package_admission(ev::ICommitStore& store,const VerifiedPackage& package,ev::CommitBatch& batch){
  PackagePolicy policy;ev::StoredRecord previous;if(auto r=load_policy(store,policy,previous);!r)return r;
  if(std::find(policy.packages.begin(),policy.packages.end(),package.identity.manifest_sha256)==policy.packages.end())policy.packages.push_back(package.identity.manifest_sha256);
  std::sort(policy.packages.begin(),policy.packages.end());
  std::set<Digest> refs;for(const auto& id:policy.packages)if(id!=package.identity.manifest_sha256)if(auto r=package_references(store,id,refs);!r)return r;
  if(auto r=binding_references(store,refs);!r)return r;
  std::map<Digest,std::uint64_t> added{{package.identity.host_sha256,package.host.size()},{package.identity.manifest_sha256,package.manifest.size()}};
  if(!package.witness.empty()){
    added.emplace(sha256(package.witness.build_manifest),package.witness.build_manifest.size());
    added.emplace(sha256(package.witness.compatibility_manifest),package.witness.compatibility_manifest.size());
  }
  for(const auto& [id,size]:added){(void)size;refs.insert(id);}
  std::uint64_t bytes=0;if(auto r=bytes_for(store,refs,bytes,added);!r)return r;
  if(policy.packages.size()>policy.maximum_packages||bytes>policy.maximum_bytes)return {Code::capacity,"package witness policy exhausted; review superseded-package retention before activation"};
  stage_policy(policy,previous,batch);return {};
}
Result package_policy(ev::ICommitStore& store,std::uint64_t count,std::uint64_t bytes,std::string_view operation,ev::CommitReceipt& receipt){
  if(!valid_token(operation,128)||count<2||count>256||bytes<65536||bytes>8ULL*1024ULL*1024ULL*1024ULL)return {Code::invalid,"package limits require 2..256 packages and 64 KiB..8 GiB"};
  const auto request=sha256(pack({"package-policy/1",std::to_string(count),std::to_string(bytes)}));
  const auto found=store.lookup_operation("package-policy-"+std::string(operation),receipt);
  if(found){if(receipt.request_digest!=request)return {Code::conflict,"policy operation differs"};receipt.duplicate=true;return {};}
  if(found.code!=Code::not_found)return found;
  PackageUsage usage;if(auto r=package_usage(store,usage);!r)return r;
  if(usage.policy.packages.size()>count||usage.retained_bytes>bytes)return {Code::capacity,"new package policy would exclude retained witnesses"};
  ev::StoredRecord previous;PackagePolicy policy;if(auto r=load_policy(store,policy,previous);!r)return r;
  policy.maximum_packages=count;policy.maximum_bytes=bytes;
  ev::CommitBatch batch;batch.operation_id="package-policy-"+std::string(operation);batch.request_digest=request;batch.expected_generation=store.generation();
  batch.commit_class=ev::CommitClass::maintenance;stage_policy(policy,previous,batch);return store.commit(batch,receipt);
}
Result package_pin(ev::ICommitStore& store,std::string_view id,bool pinned,std::string_view operation,ev::CommitReceipt& receipt){
  if(!valid_digest(id)||!valid_token(operation,128))return {Code::invalid,"invalid package pin"};
  const auto request=sha256(pack({"package-pin/1",std::string(id),pinned?"1":"0"}));
  const auto found=store.lookup_operation("package-pin-"+std::string(operation),receipt);
  if(found){if(receipt.request_digest!=request)return {Code::conflict,"pin operation differs"};receipt.duplicate=true;return {};}
  if(found.code!=Code::not_found)return found;
  std::set<Digest> refs;if(pinned)if(auto r=package_references(store,id,refs);!r)return r;
  ev::StoredRecord previous;const auto old=store.latest("operator.deployment-pin",id,previous);if(!old&&old.code!=Code::not_found)return old;
  ev::CommitBatch batch;batch.operation_id="package-pin-"+std::string(operation);batch.request_digest=request;batch.expected_generation=store.generation();
  batch.commit_class=ev::CommitClass::maintenance;batch.guards.push_back({"operator.deployment-pin",std::string(id),previous.revision,previous.digest});
  batch.records.push_back({"operator.deployment-pin",std::string(id),pack({"package-pin/1",std::string(id)}),{refs.begin(),refs.end()},!pinned});return store.commit(batch,receipt);
}
Bytes encode_package_retention(const PackageRetentionPlan& plan){return pack({retention_body(plan),plan.plan_sha256});}
Result decode_package_retention(std::string_view bytes,PackageRetentionPlan& plan){
  std::vector<std::string> outer,fields;PackageRetentionPlan next;
  if(!unpack(bytes,outer,2,32768)||outer.size()!=2||!unpack(outer[0],fields,8,32768)||fields.size()!=8||fields[0]!="anaiero.package-retention/1"||
     !number(fields[3],next.generation)||!number(fields[7],next.bytes)||!unpack(fields[5],next.packages,8,1024)||!unpack(fields[6],next.objects,32,4096))return corrupt("invalid package-retention encoding");
  next.operation_id=fields[1];next.store_id=fields[2];next.root_sha256=fields[4];next.plan_sha256=outer[1];
  if(auto r=validate(next);!r)return r;
  plan=std::move(next);return {};
}
Result package_retention_review(ev::ICommitStore& store,std::vector<Digest> packages,std::string_view operation,PackageRetentionPlan& plan){
  std::sort(packages.begin(),packages.end());if(!digests(packages,8)||packages.empty()||!valid_token(operation,128))return {Code::invalid,"select 1..8 unique exact package manifest digests"};
  PackagePolicy policy;ev::StoredRecord previous;if(auto r=load_policy(store,policy,previous);!r)return r;
  Service service(store);Selection selected;if(auto r=service.selection(selected);!r)return r;
  std::set<Digest> candidates,protected_objects;
  if(auto r=binding_references(store,protected_objects);!r)return r;
  for(const auto& id:packages){
    if(id==selected.current.manifest_sha256||id==selected.previous.manifest_sha256)return {Code::conflict,"current and previous rollback packages are protected"};
    if(std::find(policy.packages.begin(),policy.packages.end(),id)==policy.packages.end())return {Code::history_expired,"package is not in the retained active catalog"};
    ev::StoredRecord pin;const auto pinned=store.latest("operator.deployment-pin",id,pin);
    if(pinned&&!pin.record.tombstone)return {Code::conflict,"operator-pinned package is protected"};
    if(!pinned&&pinned.code!=Code::not_found)return pinned;
    if(auto r=package_references(store,id,candidates);!r)return r;
  }
  for(const auto& id:policy.packages)if(!std::binary_search(packages.begin(),packages.end(),id))if(auto r=package_references(store,id,protected_objects);!r)return r;
  for(const auto& id:protected_objects)candidates.erase(id);
  std::uint64_t bytes=0;if(auto r=bytes_for(store,candidates,bytes);!r)return r;
  if(candidates.size()>32)return {Code::capacity,"package expiry exceeds 32-object bound"};
  plan={};plan.operation_id=operation;plan.store_id=store.info().store_id;plan.generation=store.generation();plan.root_sha256=store.info().root_digest;
  plan.packages=std::move(packages);plan.objects.assign(candidates.begin(),candidates.end());plan.bytes=bytes;plan.plan_sha256=sha256(retention_body(plan));return validate(plan);
}
Result package_retention_activate(ev::ICommitStore& store,const PackageRetentionPlan& plan,ev::CommitReceipt& receipt){
  if(auto r=validate(plan);!r)return r;
  if(store.info().store_id!=plan.store_id)return {Code::conflict,"retention plan belongs to another store"};
  const auto operation="package-expire-"+plan.operation_id;const auto old=store.lookup_operation(operation,receipt);
  if(old){if(receipt.request_digest!=plan.plan_sha256)return {Code::conflict,"retention operation differs"};receipt.duplicate=true;return {};}
  if(old.code!=Code::not_found)return old;
  if(store.generation()!=plan.generation||store.info().root_digest!=plan.root_sha256)return {Code::conflict,"package retention review is stale"};
  PackageRetentionPlan checked;if(auto r=package_retention_review(store,plan.packages,plan.operation_id,checked);!r)return r;
  if(checked.plan_sha256!=plan.plan_sha256)return {Code::conflict,"package retention candidates changed"};
  PackagePolicy policy;ev::StoredRecord policy_record;if(auto r=load_policy(store,policy,policy_record);!r)return r;
  ev::CommitBatch batch;batch.operation_id=operation;batch.request_digest=plan.plan_sha256;batch.expected_generation=plan.generation;batch.commit_class=ev::CommitClass::maintenance;
  for(const auto& id:plan.packages){
    for(const auto kind:{"operator.deployment-package","operator.deployment-witness"}){
      ev::StoredRecord record;const auto found=store.latest(kind,id,record);if(found.code==Code::not_found&&std::string_view(kind)=="operator.deployment-witness")continue;if(!found)return found;
      batch.guards.push_back({kind,id,record.revision,record.digest});batch.records.push_back({kind,id,record.record.payload,{},true});
    }
    policy.packages.erase(std::remove(policy.packages.begin(),policy.packages.end(),id),policy.packages.end());
  }
  stage_policy(policy,policy_record,batch);batch.expire_blobs=plan.objects;
  batch.records.push_back({"operator.deployment-expiry",plan.operation_id,encode_package_retention(plan),{},false});return store.commit(batch,receipt);
}
Result package_retention_reclaim(ev::ICommitStore& store,std::string_view operation,std::uint64_t& remaining){
  remaining=0;if(!valid_token(operation,128))return {Code::invalid,"invalid package cleanup operation"};
  ev::StoredRecord record;if(auto r=store.latest("operator.deployment-expiry",operation,record);!r)return r;
  PackageRetentionPlan plan;if(record.record.tombstone)return corrupt("package expiry authority is retired");
  if(auto r=decode_package_retention(record.record.payload,plan);!r)return r;
  for(const auto& id:plan.objects){ev::BlobInfo blob;if(auto r=store.blob_info(id,blob);!r)return r;
    if(!blob.expired_generation||blob.expired_generation!=record.generation)return corrupt("package expiry generation differs");
    // Always reconcile even if the file is already absent after interruption.
    if(auto r=store.reclaim_expired_blob(id,store.generation());!r)return r;
  }
  return {};
}
}
