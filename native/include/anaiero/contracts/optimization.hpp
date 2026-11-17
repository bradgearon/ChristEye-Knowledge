#pragma once
// Digital-only optimization. Real-cell observations cannot authorize changes.
#include "anaiero/contracts/runtime_events.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
namespace anaiero::optimization {
enum class Status : std::uint8_t {
  ok, invalid, missing_metric, unqualified, exhausted, no_improvement,
  stale_revision, trial_rejected, commit_failed, not_committed,
  awaiting_observation, reverted, rollback_failed
};
enum class RouteKind : std::uint8_t { digital_signal=1, digital_learning=2, synthetic_resource=3 };
struct Goal {
  std::uint32_t colony=0,animon=0,source=0;
  runtime::Metric metric=runtime::Metric::digital_learning;
  std::int32_t ideal=0;
  std::uint16_t importance=1, minimum_gain=1, allowed_regression=0;
};
struct Candidate {
  std::uint32_t id=0, colony=0,from=0,to=0;
  RouteKind kind=RouteKind::digital_signal;
  std::uint16_t digital_cost=0, model_risk=0, guidance_weight=0;
};
struct TrialResult {
  bool valid=false;
  std::int32_t predicted=0;
  std::uint16_t uncertainty=0;
};
struct Decision {
  Status status=Status::invalid;
  Goal goal{};
  Candidate selected{};
  std::uint64_t snapshot_revision=0, commit_id=0;
  std::int32_t baseline=0,predicted=0;
  std::int64_t weighted_gain=0, score=0;
  bool committed=false;
  std::string source_world_sha256; // required for durable state change ports
};
// Ports may be injected by trusted simulator plugins or test fixtures.
struct ICandidateSource {
  virtual ~ICandidateSource()=default;
  virtual std::size_t count() const noexcept=0;
  virtual bool at(std::size_t index,Candidate& output) const noexcept=0;
};
struct ITrialEvaluator {
  virtual ~ITrialEvaluator()=default;
  virtual bool dry_run(const runtime::ColonyState&,const Goal&,
                       const Candidate&,TrialResult&) noexcept=0;
};
struct IRouteChangePort {
  virtual ~IRouteChangePort()=default;
  // Must validate revision, authorization and digital-only scope. Durable
  // implementations must couple audit/outbox with state commit; the demo is RAM-only.
  virtual bool commit(const Decision&,std::uint64_t& change_id) noexcept=0;
  virtual bool revert(std::uint64_t change_id) noexcept=0;
};
struct IScorePolicy {
  virtual ~IScorePolicy()=default;
  virtual std::int64_t score(const Goal&,std::int32_t baseline,
       const Candidate&,const TrialResult&) const noexcept=0;
};
} // namespace anaiero::optimization
