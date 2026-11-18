#pragma once
// Bounded immutable digital experiment definitions and durable trial progress.
#include "anaiero/contracts/engine_optimization.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
namespace anaiero::experiments {
inline constexpr std::size_t kMaxCandidates=8,kMaxStages=8,kMaxArtifacts=16;
inline constexpr std::size_t kMaxCheckpointBytes=1024*1024;
enum class Code : std::uint8_t {ok,invalid_definition,identity_mismatch,unsupported,
  evaluation_failed,persistence_failed,corrupt,invalid_state,capacity,cancelled};
enum class PolicyKind : std::uint8_t {sw5_guidance=1,sim3_link_units=2,sim3_teach=3};
enum class RunState : std::uint8_t {prepared=1,running,paused,completed,failed};
enum class TrialState : std::uint8_t {pending=0,completed,failed};
enum class OutcomeDecision : std::uint8_t {pending=0,retain,revert};
struct Candidate {
  engine::Candidate ranking{};
  PolicyKind kind=PolicyKind::sw5_guidance;
  std::string from,to;
  std::uint16_t value=0;
  std::uint8_t channel=0; // SIM3: food=1, data=2, signal=3; zero for other policies
};
struct Stage {
  std::string id;
  std::uint16_t ticks=0;
  engine::Inputs inputs{};
};
struct Artifact {
  std::string id,role,relative_path,sha256;
};
struct Definition {
  std::string name,sha256,model_sha256,behavior_id,input_schedule_sha256;
  std::uint32_t engine_id=0,metric_schema=0,clock_domain=0;
  engine::Kind kind=engine::Kind::sw5;
  std::uint64_t tick_ns=0,seed=0;
  engine::OptimizationPlan optimization{};
  std::uint8_t candidate_count=0,stage_count=0,artifact_count=0;
  std::array<Candidate,kMaxCandidates> candidates{};
  std::array<Stage,kMaxStages> stages{};
  std::array<Artifact,kMaxArtifacts> artifacts{};
  // Optional existing CEVPLAN + CEVLM2 base, validated with the original parser.
  // These pin learning provenance; their VLM actions are never reinterpreted.
  std::string learning_plan_path,learning_plan_sha256,learning_base_path,learning_base_sha256;
  std::uint16_t outcome_ticks=0;
  std::int64_t outcome_tolerance=0;
  bool automatic_apply=false,automatic_revert=false;
};
struct EvaluationContext {
  std::uint32_t engine_id=0,source=0,metric_schema=0,clock_domain=0;
  std::uint64_t source_epoch=0,source_revision=0,start_tick=0,tick_ns=0,seed=0;
  std::string source_state_sha256,model_sha256,behavior_id,input_schedule_sha256;
};
struct TrialReceipt {
  TrialState state=TrialState::pending;
  Code status=Code::ok;
  engine::Candidate candidate{};
  engine::Forecast forecast{};
  std::string receipt_sha256;
};
struct Outcome {
  OutcomeDecision decision=OutcomeDecision::pending;
  std::uint64_t change_id=0,applied_tick=0;
  engine::Snapshot observed{};
  std::array<std::int64_t,4> residuals{};
};
struct Record {
  std::uint64_t revision=0;
  std::string operation_id;
  Definition definition{};
  EvaluationContext context{};
  std::string source_checkpoint;
  RunState state=RunState::prepared;
  Code failure=Code::ok;
  std::uint8_t next_candidate=0;
  std::array<TrialReceipt,kMaxCandidates> trials{};
  engine::Selection selection{};
  Outcome outcome{};
};
struct ICancellation {
  virtual ~ICancellation()=default;
  virtual bool cancelled()const noexcept=0;
};
struct IEvaluator {
  virtual ~IEvaluator()=default;
  // An evaluator is bound to an immutable source checkpoint and descriptor.
  virtual Code context(const Definition&,EvaluationContext&)const noexcept=0;
  virtual Code evaluate(const Definition&,const EvaluationContext&,std::size_t candidate,
                        engine::Forecast&,const ICancellation*)noexcept=0;
};
struct IRecordStore {
  virtual ~IRecordStore()=default;
  // Verify the canonical definition digest at the persistence boundary.
  virtual bool accepts_definition(const Definition&)const noexcept=0;
  virtual bool load(std::string_view operation_id,Record&,bool& exists)noexcept=0;
  // CAS save increments revision only after durable success.
  virtual bool save(Record&)noexcept=0;
};
} // namespace anaiero::experiments
