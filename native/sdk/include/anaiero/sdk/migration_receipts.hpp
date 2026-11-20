#pragma once
// Typed persistence adapter. Raw canonical records never reach the runtime
// migration policy or the operator's migration-status branch.
#include "anaiero/sdk/evidence.hpp"
#include "anaiero/sdk/migration_receipt_codec.hpp"

namespace anaiero::sdk::evidence {
struct StoredMigrationReceipt {
  MigrationReceipt receipt;
  std::uint64_t generation=0;
};
Result stage_migration_receipt(CommitBatch&,const MigrationReceipt&);
Result read_migration_receipt(IEvidenceIndex&,std::string_view operation,StoredMigrationReceipt&);
}
