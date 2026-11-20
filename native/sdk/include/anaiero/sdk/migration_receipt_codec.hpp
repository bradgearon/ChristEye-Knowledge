#pragma once
// Migration receipt wire format. No storage, CLI or runtime service ownership.
#include "anaiero/sdk/engine.hpp"

namespace anaiero::sdk {
struct MigrationReceipt {
  MigrationPlan plan;
  Digest transition_sha256;
};
Bytes encode_migration_receipt(const MigrationReceipt&);
Result decode_migration_receipt(std::string_view,MigrationReceipt&);
}
