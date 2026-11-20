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
}
