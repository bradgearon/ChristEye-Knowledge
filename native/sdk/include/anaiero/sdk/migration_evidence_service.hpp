#pragma once
// Migration evidence and integrity orchestration. SessionService owns lifecycle
// policy and serialized execution; this service owns all migration hashes,
// canonical witnesses, stored receipts, and exact retry verification.
#include "anaiero/sdk/migration_receipts.hpp"

namespace anaiero::sdk::evidence {
class MigrationEvidenceService final {
  ICommitStore& store_;
public:
  explicit MigrationEvidenceService(ICommitStore& store) noexcept:store_(store){}
  // Produce the exact pseudo-request used by the canonical transition
  // validator without exposing length-prefixed wire details to the runtime.
  Result request_step(const EngineIdentity&,const MigrationRequest&,StepRequest&) const;
  Result review(const EngineIdentity&,const EngineState&,const MigrationRequest&,
                const PreparedTransition&,MigrationPlan&) const;
  Result verify_plan(const MigrationPlan&) const;
  Result match_plans(const MigrationPlan&,const MigrationPlan&) const;
  Result append_receipt(PreparedTransition&,const MigrationPlan&) const;
  Result stage(CommitBatch&,const EngineIdentity&,const EngineState&,
               const PreparedTransition&,const MigrationPlan&) const;
  // Inspect the actual canonical operation receipt and typed migration record
  // before accepting a retry. Never confuse a missing record with success.
  Result accepted_retry(const MigrationPlan&,bool& duplicate) const;
};
} // namespace anaiero::sdk::evidence
