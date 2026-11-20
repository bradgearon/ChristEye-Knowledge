#include "anaiero/sdk/migration_evidence_service.hpp"

namespace anaiero::sdk::evidence {
Result MigrationEvidenceService::request_step(const EngineIdentity& identity,
                                              const MigrationRequest& request,
                                              StepRequest& output) const {
  if(!valid_identity(identity))return {Code::invalid,"migration engine identity"};
  const auto bytes=encode_migration_request(request);
  if(bytes.empty())return {Code::invalid,"invalid migration request"};
  output={request.operation_id,request.expected_revision,{"anaiero.engine-migration/1",bytes}};
  return {};
}
Result MigrationEvidenceService::review(const EngineIdentity& identity,const EngineState& before,
                                        const MigrationRequest& request,
                                        const PreparedTransition& prepared,MigrationPlan& output) const {
  if(prepared.identity!=identity||prepared.operation_id!=request.operation_id||
     prepared.request_sha256!=migration_request_digest(identity,request))
    return {Code::corrupt,"migration review request witness mismatch"};
  const auto source=checkpoint_digest(identity,before);
  const auto target=checkpoint_digest(identity,prepared.target);
  MigrationPlan plan{identity,request,source,target};
  if(encode_migration_plan(plan).empty())
    return {Code::corrupt,"migration review witness invalid"};
  output=std::move(plan);return {};
}
Result MigrationEvidenceService::verify_plan(const MigrationPlan& plan) const {
  if(encode_migration_plan(plan).empty())return {Code::invalid,"invalid pinned migration plan"};
  return {};
}
Result MigrationEvidenceService::match_plans(const MigrationPlan& reviewed,
                                              const MigrationPlan& expected) const {
  const auto canonical=encode_migration_plan(reviewed),pinned=encode_migration_plan(expected);
  if(canonical.empty()||pinned.empty())return {Code::invalid,"invalid migration plan"};
  if(canonical!=pinned)return {Code::conflict,"migration plan is stale or transformation changed"};
  return {};
}
Result MigrationEvidenceService::append_receipt(PreparedTransition& transition,
                                                const MigrationPlan& plan) const {
  if(auto result=verify_plan(plan);!result)return result;
  transition.receipts.push_back({"anaiero.engine-migration-receipt/1",
    pack({plan.request.implementation_sha256,plan.source_checkpoint_sha256,
          plan.target_checkpoint_sha256})});
  return {};
}
Result MigrationEvidenceService::stage(CommitBatch& batch,const EngineIdentity& identity,
                                      const EngineState& previous,const PreparedTransition& transition,
                                      const MigrationPlan& plan) const {
  if(auto result=verify_plan(plan);!result)return result;
  if(plan.identity!=identity||transition.identity!=identity)
    return {Code::conflict,"migration source identity changed"};
  const auto prior=checkpoint_digest(identity,previous);
  const auto next=checkpoint_digest(transition.identity,transition.target);
  if(prior!=plan.source_checkpoint_sha256||next!=plan.target_checkpoint_sha256)
    return {Code::conflict,"migration state or review witness changed"};
  const auto encoded=encode_transition(transition);
  if(encoded.empty())return {Code::capacity,"migration transition encoding bound"};
  return stage_migration_receipt(batch,{plan,sha256(encoded)});
}
Result MigrationEvidenceService::accepted_retry(const MigrationPlan& plan,bool& duplicate) const {
  duplicate=false;
  if(auto result=verify_plan(plan);!result)return result;
  CommitReceipt receipt;
  const auto found=store_.lookup_operation(plan.request.operation_id,receipt);
  if(found.code==Code::not_found)return {};
  if(!found)return found;
  if(receipt.request_digest!=migration_request_digest(plan.identity,plan.request))
    return {Code::conflict,"operation ID reused with changed request bytes"};
  StoredMigrationReceipt accepted;
  if(auto result=read_migration_receipt(store_,plan.request.operation_id,accepted);!result)
    return result;
  if(auto result=match_plans(accepted.receipt.plan,plan);!result)
    return {Code::conflict,"operation was accepted with a different migration review"};
  duplicate=true;return {};
}
} // namespace anaiero::sdk::evidence
