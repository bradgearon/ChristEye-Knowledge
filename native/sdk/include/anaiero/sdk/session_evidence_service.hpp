#pragma once
// Serialized session evidence: records, content-addressed checkpoints and
// canonical commit witnesses. SessionService retains policy/engine ownership.
#include "anaiero/sdk/evidence.hpp"
#include "anaiero/sdk/session_record_codec.hpp"

namespace anaiero::sdk::evidence {
class SessionEvidenceService final {
  ICommitStore& store_;
public:
  explicit SessionEvidenceService(ICommitStore& store) noexcept:store_(store){}
  Result stage_transition(const PreparedTransition&,std::uint64_t record_revision,
                          const Digest& record_digest,const EngineState& committed_state,
                          CommitBatch&,Record& latest) const;
  Result read_checkpoint(const Digest& checkpoint_sha256,const EngineIdentity& expected,
                         EngineState& output) const;
  Result registration(const EngineIdentity&,const EngineState&,PreparedTransition&) const;
  Digest phase_request(const EngineIdentity&,std::string_view operation,Phase) const;
  Digest detach_request(const EngineIdentity&) const;
  std::string metric_prefix(const EngineIdentity&) const;
};
} // namespace anaiero::sdk::evidence
