#include "anaiero/sdk/migration_receipts.hpp"

namespace anaiero::sdk::evidence {
namespace {
constexpr std::string_view migration_kind="sdk.engine-migration";
}
Result stage_migration_receipt(CommitBatch& batch,const MigrationReceipt& receipt) {
  const auto bytes=encode_migration_receipt(receipt);
  const auto& plan=receipt.plan;
  if(bytes.empty()||batch.operation_id!=plan.request.operation_id||
     batch.request_digest!=migration_request_digest(plan.identity,plan.request))
    return {Code::invalid,"migration receipt does not match canonical operation"};
  // Prevent accidentally adding more than one migration receipt in one batch.
  for(const auto& record:batch.records)
    if(record.kind==migration_kind)return {Code::conflict,"duplicate migration receipt"};
  batch.guards.push_back({std::string(migration_kind),plan.request.operation_id,0,{}});
  batch.records.push_back({std::string(migration_kind),plan.request.operation_id,bytes,
    {plan.source_checkpoint_sha256,plan.target_checkpoint_sha256},false});
  return {};
}
Result read_migration_receipt(IEvidenceIndex& index,std::string_view operation,StoredMigrationReceipt& output) {
  StoredRecord stored;
  const auto found=index.latest(migration_kind,operation,stored);
  if(!found)return found;
  MigrationReceipt receipt;
  if(stored.record.tombstone||stored.record.kind!=migration_kind||
     stored.record.key!=operation||
     !decode_migration_receipt(stored.record.payload,receipt)||
     receipt.plan.request.operation_id!=operation||
     stored.record.references!=std::vector<Digest>{
       receipt.plan.source_checkpoint_sha256,receipt.plan.target_checkpoint_sha256})
    return {Code::corrupt,"invalid migration evidence record"};
  output={std::move(receipt),stored.generation};return {};
}
} // namespace anaiero::sdk::evidence
