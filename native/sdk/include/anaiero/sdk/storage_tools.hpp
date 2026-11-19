#pragma once
#include "anaiero/sdk/evidence.hpp"

namespace anaiero::sdk::evidence {
struct MigrationReport {
  Digest source_fingerprint,destination_root;
  std::string backup_directory;
  std::uint64_t source_files=0,source_bytes=0,objects=0,events=0;
  std::uint64_t target_bytes=0,peak_required_bytes=0,retained_backup_bytes=0;
  bool resumed=false,already_complete=false,source_unchanged=false,activated=false;
};
// Quiescent evidence-only migration. Preserves all verified legacy CAS objects,
// exact committed event wire bytes and provenance; does not upgrade live engine
// state. A separate immutable source snapshot remains beside the destination and
// is explicitly reported outside the canonical store's quota.
Result migrate_legacy_evidence(std::string_view source,std::string_view destination,
    const StoreOptions&,MigrationReport&);

struct CompactionOptions {
  std::string operation_id;
  std::uint64_t expected_generation=any_generation;
  Digest expected_root;
  // Additional temporary bytes, separately checked against store and filesystem
  // headroom. Pages and index work remain bounded while the history is scanned.
  std::uint64_t working_budget_bytes=256ULL*1024ULL*1024ULL;
  bool expire_operation_receipts=false;
  // Automatic preserves the current layout. An explicit change is bound to
  // the reviewed plan and built in the inactive index directory.
  IndexLayout target_index_layout=IndexLayout::automatic;
};
struct CompactionPlan {
  CompactionOptions options;
  Digest plan_digest,source_root,source_base;
  std::uint64_t generation=0,next_operation_epoch=0;
  std::uint64_t accounted_bytes_before=0,required_working_bytes=0;
  std::uint64_t retained_records=0,retained_blobs=0,retained_operations=0;
  std::uint64_t summarized_record_versions=0,expired_observations=0;
  std::uint64_t canonical_transactions_removed=0;
};
struct CompactionReport {
  Digest plan_digest,canonical_base_digest;
  std::uint64_t generation=0,operation_epoch=0;
  std::uint64_t accounted_bytes_before=0,accounted_bytes_after=0;
  std::uint64_t removed_transaction_bytes=0,removed_index_bytes=0;
  std::uint64_t summarized_record_versions=0,expired_observations=0;
  bool activated=false,cleanup_complete=false,already_complete=false;
  // Sampled after both complete indexes exist, before older files are removed.
  // Directory count excludes the store root. Only reported by the measured run.
  bool coexistence_measured=false;
  std::uint64_t coexistence_files=0,coexistence_directories=0;
  std::uint64_t coexistence_old_index_files=0,coexistence_new_index_files=0;
};
// A maintenance scan creates a deterministic plan. Execution revalidates the
// exact root/index/base and recomputes the plan before publication. All latest
// records (including tombstones/authority), blob identity fences and source tips
// remain protected; arbitrary identity cardinality still consumes finite quota.
Result preview_native_compaction(ICommitStore&,const CompactionOptions&,CompactionPlan&);
Result execute_native_compaction(ICommitStore&,const CompactionPlan&,CompactionReport&);
Result resume_native_compaction(ICommitStore&,CompactionReport&);
// Explicitly create a NEW operation identity after an operator rotates receipt
// history. Never use this to relabel an unknown retry from an earlier epoch.
std::string qualify_operation_id(std::uint64_t epoch,std::string_view logical_id);
// Read-only format/epoch inspection for a caller holding a quiescent store lock.
// Does not open a writer, repair a cache, or initialize missing storage.
Result inspect_native_operation_epoch(std::string_view directory,std::uint64_t& epoch);
}
