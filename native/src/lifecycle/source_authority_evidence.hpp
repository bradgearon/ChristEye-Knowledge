#pragma once
// Immutable local-vs-relay ownership evidence and its original binary format.
// Keeps UID hashing, foreign-role fences and canonical record CAS guards outside
// the lifecycle orchestrator. Retains source-authority/1 exact persisted bytes.
#include "anaiero/sdk/lifecycle.hpp"

namespace anaiero::sdk::lifecycle {
class SourceAuthorityEvidenceService final {
  evidence::ICommitStore& store_;
public:
  explicit SourceAuthorityEvidenceService(evidence::ICommitStore& store) noexcept:store_(store){}
  Result read(std::string_view uid,SourceAuthority&) const;
  Result stage(evidence::CommitBatch&,const SourceAuthority&) const;
};
}
