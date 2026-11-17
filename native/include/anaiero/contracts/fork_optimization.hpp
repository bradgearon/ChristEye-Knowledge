#pragma once
// Versioned, digital-only bounded trial and route-transaction records.
// No real culture control or physiological stimulation.
#include "anaiero/contracts/optimization.hpp"
#include "anaiero/models/cell_behavior.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace anaiero::optimization {
inline constexpr std::size_t kForkMaxGoals=6;
inline constexpr std::size_t kForkMaxCandidates=8;
inline constexpr std::uint16_t kForkMaxHorizon=128;
enum class Measure : std::uint8_t {
  viable=1, directed_links=2, total_energy=3, total_activity=4,
  total_learning=5, total_process=6
};
struct Objective {
  Measure metric=Measure::directed_links;
  std::int64_t ideal=0;
  std::uint16_t importance=1;
  bool forbid_regression=false;
};
struct ForkPlan {
  std::array<Objective,kForkMaxGoals> objectives{};
  std::uint8_t objective_count=0;
  std::uint16_t horizon=0;
  cells::Context trial_context{};
  std::int64_t minimum_score=1;
};
struct ForkOption { Candidate route{};std::uint16_t guidance=0,uncertainty=0; };
struct ForkMetrics {
  std::uint32_t viable=0,links=0;
  std::int64_t energy=0,activity=0,learning=0,process=0;
};
enum class ForkStatus : std::uint8_t {
  ok, invalid, invalid_world, trial_failed, no_improvement,
  source_changed, storage_failed, pending_receipt, invalid_event
};
struct ForkSelection {
  ForkStatus status=ForkStatus::invalid;
  ForkOption option{};
  ForkMetrics original{},baseline_forecast{},candidate_forecast{};
  std::string initial_world_sha256;
  std::uint64_t snapshot_revision=0;
  std::int64_t score=0;
};
enum class RouteEventKind : std::uint8_t {none=0, route_committed=1,
  route_reverted=2, simulation_advanced=3};
struct DigitalRouteLedger {
  std::uint64_t revision=0,observed_revision=0,change_id=0,event_sequence=0;
  bool active=false,prior_existed=false,event_acked=true;
  std::uint16_t applied_guidance=0,prior_guidance=0;
  Candidate selected{};
  RouteEventKind event_kind=RouteEventKind::none;
  cells::World initial_world{},current_world{};
  std::string atlas_hash,parent_hash,head_hash,current_world_sha256;
};
struct RouteReceipt {
  RouteEventKind kind=RouteEventKind::none;std::uint64_t sequence=0,change_id=0,revision=0;
  Candidate candidate{};std::string state_sha256;
};
struct IRouteReceiptSink {virtual ~IRouteReceiptSink()=default;
  virtual bool publish(const RouteReceipt& receipt) noexcept=0;};
enum class StoreResult : std::uint8_t {
  ok,not_found,already_exists,bad_state,corrupt,locked,io_error,conflict,capacity
};
struct IAtomicRouteLedger {
  virtual ~IAtomicRouteLedger()=default;
  virtual StoreResult initialize(const DigitalRouteLedger& start,
                                 DigitalRouteLedger& written) noexcept=0;
  virtual StoreResult read(DigitalRouteLedger& state) noexcept=0;
  // Commit one world + route + evidence receipt as a single pointed-to blob.
  virtual StoreResult compare_exchange(const DigitalRouteLedger& prior,
                      DigitalRouteLedger& next) noexcept=0;
};
} // namespace anaiero::optimization
