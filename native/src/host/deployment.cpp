#include "deployment.hpp"
#include <algorithm>
#include <charconv>

namespace anaiero::deployment {
namespace ev=evidence;
namespace {
bool number(std::string_view text,std::uint64_t& value){
  const auto parsed=std::from_chars(text.data(),text.data()+text.size(),value);
  return !text.empty()&&parsed.ec==std::errc{}&&parsed.ptr==text.data()+text.size()&&std::to_string(value)==text;
}
bool valid(const Package& p){
  return !p.directory.empty()&&p.directory.size()<=32768&&p.directory.find('\0')==p.directory.npos&&
    valid_digest(p.manifest_sha256)&&valid_digest(p.host_sha256)&&valid_digest(p.module_sha256);
}
Bytes package_wire(const Package& p){return pack({p.directory,p.manifest_sha256,p.host_sha256,p.module_sha256});}
Result package_read(std::string_view bytes,Package& p,bool empty=false){
  std::vector<std::string> f;
  if(!unpack(bytes,f,4,40000)||f.size()!=4)return {Code::corrupt,"invalid deployment package identity"};
  p={f[0],f[1],f[2],f[3]};
  if(empty&&p==Package{})return {};
  return valid(p)?Result{}:Result{Code::corrupt,"invalid deployment package bounds"};
}
Bytes body(const Plan& p){
  return pack({"anaiero.deployment-plan/1",p.operation_id,p.store_id,std::to_string(p.generation),p.root_sha256,
    std::to_string(p.selection_revision),p.selection_sha256,package_wire(p.candidate),package_wire(p.previous)});
}
Result validate(const Plan& p){
  if(!valid_token(p.operation_id,128)||!valid_digest(p.store_id)||
     (p.generation==0?!p.root_sha256.empty():!valid_digest(p.root_sha256))||
     !valid(p.candidate)||(p.previous!=Package{}&&!valid(p.previous))||
     (p.previous!=Package{}&&p.previous.module_sha256!=p.candidate.module_sha256)||
     (p.selection_revision==0?!p.selection_sha256.empty():!valid_digest(p.selection_sha256))||
     !valid_digest(p.plan_sha256)||sha256(body(p))!=p.plan_sha256)
    return {Code::corrupt,"deployment plan identity or digest is invalid"};
  return {};
}
std::string operation(const Plan& p){return "deployment-"+p.operation_id;}
}
Bytes encode_plan(const Plan& p){return pack({body(p),p.plan_sha256});}
Result decode_plan(std::string_view bytes,Plan& p){
  std::vector<std::string> outer,f;Plan next;
  if(!unpack(bytes,outer,2,128U*1024U)||outer.size()!=2||
     !unpack(outer[0],f,9,128U*1024U)||f.size()!=9||f[0]!="anaiero.deployment-plan/1"||
     !number(f[3],next.generation)||!number(f[5],next.selection_revision))
    return {Code::corrupt,"invalid deployment plan artifact"};
  next.operation_id=f[1];next.store_id=f[2];next.root_sha256=f[4];next.selection_sha256=f[6];next.plan_sha256=outer[1];
  if(auto r=package_read(f[7],next.candidate);!r)return r;
  if(auto r=package_read(f[8],next.previous,true);!r)return r;
  if(auto r=validate(next);!r)return r;
  p=std::move(next);return {};
}
Result Service::selection(Selection& out){
  out={};ev::StoredRecord record;const auto found=store_.latest("operator.deployment","selection",record);
  if(found.code==Code::not_found)return {};
  if(!found)return found;
  std::vector<std::string> f;
  if(record.record.tombstone||!unpack(record.record.payload,f,6,128U*1024U)||f.size()!=6||
     f[0]!="anaiero.deployment-selection/1"||f[1]!=store_.info().store_id||!valid_token(f[4],128)||!valid_digest(f[5]))
    return {Code::corrupt,"invalid canonical deployment selection"};
  if(auto r=package_read(f[2],out.current);!r)return r;
  if(auto r=package_read(f[3],out.previous,true);!r)return r;
  out.operation_id=f[4];out.plan_sha256=f[5];out.revision=record.revision;out.generation=record.generation;out.record_sha256=record.digest;
  return {};
}
Result Service::retained(const Package& package){
  ev::StoredRecord record;if(auto r=store_.latest("operator.deployment-package",package.manifest_sha256,record);!r)return r;
  const auto expected=pack({"anaiero.deployment-package/1",package.module_sha256,package.host_sha256,package.manifest_sha256});
  if(record.record.tombstone||record.record.payload!=expected)return {Code::corrupt,"canonical retained package witness differs"};
  Bytes bytes;if(auto r=store_.get_blob(package.host_sha256,bytes);!r)return r;
  if(sha256(bytes)!=package.host_sha256)return {Code::corrupt,"canonical retained host bytes differ"};
  if(auto r=store_.get_blob(package.manifest_sha256,bytes);!r)return r;
  if(sha256(bytes)!=package.manifest_sha256)return {Code::corrupt,"canonical retained package manifest differs"};
  return {};
}
Result Service::review(const Package& candidate,std::string_view requested,Plan& out){
  if(!valid(candidate)||!valid_token(requested,128))return {Code::invalid,"invalid candidate package or deployment operation"};
  if(store_.info().operation_epoch!=0)return {Code::unsupported,"deployment retains the original canonical operation namespace"};
  Selection selected;if(auto r=selection(selected);!r)return r;
  if(selected.revision)if(auto r=retained(selected.current);!r)return r;
  if(selected.revision&&selected.current.module_sha256!=candidate.module_sha256)
    return {Code::unsupported,"engine implementation migration is not supported by package selection"};
  out={};out.operation_id=requested;out.store_id=store_.info().store_id;out.generation=store_.generation();
  out.root_sha256=store_.info().root_digest;out.selection_revision=selected.revision;out.selection_sha256=selected.record_sha256;
  out.candidate=candidate;out.previous=selected.current;out.plan_sha256=sha256(body(out));
  ev::CommitReceipt old;const auto found=store_.lookup_operation(operation(out),old);
  if(found)return {Code::conflict,"deployment operation already exists; retry its original plan instead of reviewing again"};
  if(found.code!=Code::not_found)return found;
  return validate(out);
}
Result Service::reconcile(const Plan& plan,ev::CommitReceipt& receipt){
  receipt={};if(auto r=validate(plan);!r)return r;
  if(store_.info().store_id!=plan.store_id)return {Code::conflict,"deployment plan belongs to another canonical store"};
  const auto found=store_.lookup_operation(operation(plan),receipt);
  if(!found)return found;
  if(receipt.request_digest!=plan.plan_sha256)return {Code::conflict,"deployment operation has another reviewed request"};
  receipt.duplicate=true;return {};
}
Result Service::activate(const Plan& plan,const VerifiedPackage& package,ev::CommitReceipt& receipt){
  const auto previous=reconcile(plan,receipt);if(previous)return previous;
  if(previous.code!=Code::not_found)return previous;
  if(package.identity!=plan.candidate||sha256(package.manifest)!=plan.candidate.manifest_sha256||sha256(package.host)!=plan.candidate.host_sha256)
    return {Code::conflict,"verified package differs from the exact reviewed bytes"};
  if(store_.generation()!=plan.generation||store_.info().root_digest!=plan.root_sha256)
    return {Code::conflict,"canonical store advanced after deployment review; review the current root"};
  Selection current;if(auto r=selection(current);!r)return r;
  if(current.revision!=plan.selection_revision||current.record_sha256!=plan.selection_sha256||current.current!=plan.previous)
    return {Code::conflict,"deployment selection changed after review"};
  if(current.revision)if(auto r=retained(current.current);!r)return r;
  ev::CommitBatch batch;batch.operation_id=operation(plan);batch.request_digest=plan.plan_sha256;batch.expected_generation=plan.generation;
  batch.guards.push_back({"operator.deployment","selection",plan.selection_revision,plan.selection_sha256});
  const auto retained=pack({"anaiero.deployment-package/1",plan.candidate.module_sha256,plan.candidate.host_sha256,plan.candidate.manifest_sha256});
  // Re-supplying an existing identity verifies its exact canonical bytes too;
  // rollback must not rely only on the external installation being healthy.
  batch.blobs.push_back({plan.candidate.host_sha256,package.host,{}});
  batch.blobs.push_back({plan.candidate.manifest_sha256,package.manifest,{}});
  ev::StoredRecord old;const auto found=store_.latest("operator.deployment-package",plan.candidate.manifest_sha256,old);
  if(found){if(old.record.tombstone||old.record.payload!=retained)return {Code::conflict,"retained package identity differs"};}
  else if(found.code!=Code::not_found)return found;
  else{
    batch.guards.push_back({"operator.deployment-package",plan.candidate.manifest_sha256,0,{}});
    batch.records.push_back({"operator.deployment-package",plan.candidate.manifest_sha256,retained,
      {plan.candidate.host_sha256,plan.candidate.manifest_sha256},false});
  }
  std::vector<Digest> references{plan.candidate.host_sha256,plan.candidate.manifest_sha256};
  if(plan.previous!=Package{}){
    references.push_back(plan.previous.host_sha256);references.push_back(plan.previous.manifest_sha256);
  }
  std::sort(references.begin(),references.end());references.erase(std::unique(references.begin(),references.end()),references.end());
  batch.records.push_back({"operator.deployment","selection",pack({"anaiero.deployment-selection/1",plan.store_id,
    package_wire(plan.candidate),package_wire(plan.previous),plan.operation_id,plan.plan_sha256}),std::move(references),false});
  return store_.commit(batch,receipt);
}
}
