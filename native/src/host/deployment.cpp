#include "deployment.hpp"
#include "deployment_record_codec.hpp"
#include "package_retention.hpp"
#include <algorithm>
#include <charconv>

namespace anaiero::deployment {
namespace ev=evidence;
using recordcodec::valid;using recordcodec::validate;using recordcodec::body;
using recordcodec::package_wire;using recordcodec::package_read;using recordcodec::operation;

Result Service::selection(Selection& out){
  out={};ev::StoredRecord record;const auto found=store_.latest("operator.deployment","selection",record);
  if(found.code==Code::not_found)return {};
  if(!found)return found;
  if(record.record.tombstone)return {Code::corrupt,"canonical deployment selection is retired"};
  if(auto r=recordcodec::decode_selection(record.record.payload,store_.info().store_id,out);!r)return r;
  out.revision=record.revision;out.generation=record.generation;out.record_sha256=record.digest;
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
Result Service::witness(const Package& package,Witness& output){
  output={};ev::StoredRecord record;
  if(auto r=store_.latest("operator.deployment-witness",package.manifest_sha256,record);!r)return r;
  std::vector<std::string> fields;
  if(record.record.tombstone||!unpack(record.record.payload,fields,5,1024)||fields.size()!=5||fields[0]!="anaiero.package-witness/1"||
     fields[1]!=package.module_sha256||!valid_digest(fields[2])||!valid_digest(fields[3])||!valid_digest(fields[4]))
    return {Code::corrupt,"invalid retained package source witness"};
  auto references=record.record.references;std::sort(references.begin(),references.end());
  std::vector<Digest> expected{fields[3],fields[4]};std::sort(expected.begin(),expected.end());
  if(references!=expected)return {Code::corrupt,"retained source witness references are incomplete"};
  Witness next;next.build_id=fields[1];next.compatibility_id=fields[2];
  if(auto r=store_.get_blob(fields[3],next.build_manifest);!r)return r.code==Code::not_found?Result{Code::corrupt,"retained whole-build witness is missing"}:r;
  if(auto r=store_.get_blob(fields[4],next.compatibility_manifest);!r)return r.code==Code::not_found?Result{Code::corrupt,"retained compatibility witness is missing"}:r;
  if(sha256(next.build_manifest)!=fields[3]||sha256(next.compatibility_manifest)!=fields[4])return {Code::corrupt,"retained package source witness changed"};
  if(auto r=validate_witness(next);!r)return r;
  output=std::move(next);return {};
}
Result Service::review_impl(const Package& candidate,std::string_view requested,Plan& out,bool compatible){
  if(!valid(candidate)||!valid_token(requested,128))return {Code::invalid,"invalid candidate package or deployment operation"};
  if(store_.info().operation_epoch!=0)return {Code::unsupported,"deployment retains the original canonical operation namespace"};
  ev::StoredRecord candidate_record;const auto candidate_found=store_.latest("operator.deployment-package",candidate.manifest_sha256,candidate_record);
  if(candidate_found&&candidate_record.record.tombstone)return {Code::history_expired,"package witnesses were explicitly retired"};
  if(!candidate_found&&candidate_found.code!=Code::not_found)return candidate_found;
  Selection selected;if(auto r=selection(selected);!r)return r;
  if(selected.revision)if(auto r=retained(selected.current);!r)return r;
  if(selected.revision&&selected.current.module_sha256!=candidate.module_sha256&&!compatible)
    return {Code::unsupported,"engine implementation migration is not supported by package selection"};
  out={};out.operation_id=requested;out.store_id=store_.info().store_id;out.generation=store_.generation();
  out.root_sha256=store_.info().root_digest;out.selection_revision=selected.revision;out.selection_sha256=selected.record_sha256;
  out.candidate=candidate;out.previous=selected.current;out.plan_sha256=sha256(body(out));
  ev::CommitReceipt old;const auto found=store_.lookup_operation(operation(out),old);
  if(found)return {Code::conflict,"deployment operation already exists; retry its original plan instead of reviewing again"};
  if(found.code!=Code::not_found)return found;
  return compatible?Result{}:validate(out);
}
Result Service::review(const Package& candidate,std::string_view operation_id,Plan& out){
  return review_impl(candidate,operation_id,out,false);
}
Result Service::review(const VerifiedPackage& package,std::string_view operation_id,Plan& out){
  ev::CommitBatch preflight;if(auto r=stage_package_admission(store_,package,preflight);!r)return r;
  Selection selected;if(auto r=selection(selected);!r)return r;
  if(!selected.revision)return review(package.identity,operation_id,out);
  if(selected.current.module_sha256==package.identity.module_sha256){
    Witness previous;const auto existing=witness(selected.current,previous);
    if(!existing&&existing.code!=Code::not_found)return existing;
    if(existing&&(package.witness.empty()||previous.compatibility_id!=package.witness.compatibility_id))
      return {Code::unsupported,"same-build selection cannot discard or change a retained compatibility contract"};
    return review(package.identity,operation_id,out);
  }
  if(package.witness.empty())return {Code::unsupported,"cross-build migration requires native compatibility witnesses on both packages"};
  Witness previous;const auto retained_witness=witness(selected.current,previous);
  if(retained_witness.code==Code::not_found)return {Code::unsupported,"precontract package cannot be rebound without a qualified source witness"};
  if(!retained_witness)return retained_witness;
  Binding binding;if(auto r=prepare_binding(store_,package.witness,previous,binding);!r)return r;
  if(auto r=review_impl(package.identity,operation_id,out,true);!r)return r;
  out.compatibility_binding=encode_binding(binding);out.plan_sha256=sha256(body(out));return validate(out);
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
  const auto prior_receipt=reconcile(plan,receipt);if(prior_receipt)return prior_receipt;
  if(prior_receipt.code!=Code::not_found)return prior_receipt;
  if(package.identity!=plan.candidate||sha256(package.manifest)!=plan.candidate.manifest_sha256||sha256(package.host)!=plan.candidate.host_sha256)
    return {Code::conflict,"verified package differs from the exact reviewed bytes"};
  if(store_.generation()!=plan.generation||store_.info().root_digest!=plan.root_sha256)
    return {Code::conflict,"canonical store advanced after deployment review; review the current root"};
  Selection current;if(auto r=selection(current);!r)return r;
  if(current.revision!=plan.selection_revision||current.record_sha256!=plan.selection_sha256||current.current!=plan.previous)
    return {Code::conflict,"deployment selection changed after review"};
  if(current.revision)if(auto r=retained(current.current);!r)return r;
  if(current.revision&&current.current.module_sha256==package.identity.module_sha256){
    Witness previous;const auto existing=witness(current.current,previous);
    if(!existing&&existing.code!=Code::not_found)return existing;
    if(existing&&(package.witness.empty()||previous.compatibility_id!=package.witness.compatibility_id))
      return {Code::unsupported,"activation cannot discard or change the retained compatibility contract"};
  }
  ev::CommitBatch batch;batch.operation_id=operation(plan);batch.request_digest=plan.plan_sha256;batch.expected_generation=plan.generation;
  if(auto r=stage_package_admission(store_,package,batch);!r)return r;
  batch.guards.push_back({"operator.deployment","selection",plan.selection_revision,plan.selection_sha256});
  const auto retained=pack({"anaiero.deployment-package/1",plan.candidate.module_sha256,plan.candidate.host_sha256,plan.candidate.manifest_sha256});
  // Re-supplying an existing identity verifies its exact canonical bytes too;
  // rollback must not rely only on the external installation being healthy.
  batch.blobs.push_back({plan.candidate.host_sha256,package.host,{}});
  batch.blobs.push_back({plan.candidate.manifest_sha256,package.manifest,{}});
  if(!package.witness.empty()){
    auto source=package.witness;if(auto r=validate_witness(source);!r)return r;
    if(source.build_id!=package.identity.module_sha256)return {Code::conflict,"package source witness has another exact build"};
    batch.blobs.push_back({source.build_sha256,source.build_manifest,{}});
    if(source.compatibility_sha256!=source.build_sha256)batch.blobs.push_back({source.compatibility_sha256,source.compatibility_manifest,{}});
    const auto payload=pack({"anaiero.package-witness/1",source.build_id,source.compatibility_id,source.build_sha256,source.compatibility_sha256});
    ev::StoredRecord existing;const auto found_witness=store_.latest("operator.deployment-witness",package.identity.manifest_sha256,existing);
    if(found_witness){if(existing.record.tombstone||existing.record.payload!=payload)return {Code::conflict,"retained package source witness differs"};}
    else if(found_witness.code!=Code::not_found)return found_witness;
    else{
      batch.guards.push_back({"operator.deployment-witness",package.identity.manifest_sha256,0,{}});
      batch.records.push_back({"operator.deployment-witness",package.identity.manifest_sha256,payload,{source.build_sha256,source.compatibility_sha256},false});
    }
  }
  if(!plan.compatibility_binding.empty()){
    Witness previous;if(auto r=witness(current.current,previous);!r)return r;
    Binding binding;if(auto r=prepare_binding(store_,package.witness,previous,binding);!r)return r;
    if(encode_binding(binding)!=plan.compatibility_binding)return {Code::conflict,"compatibility witnesses differ from the reviewed binding"};
    if(auto r=stage_binding(store_,binding,batch);!r)return r;
  }
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
