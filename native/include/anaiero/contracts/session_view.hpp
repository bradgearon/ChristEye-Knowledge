#pragma once
// Passive operator view. Its watermarks describe per-engine publication, not a
// global atomic transaction across independent engines.
#include "anaiero/contracts/simulation_engine.hpp"
#include "anaiero/contracts/experiment.hpp"
#include "anaiero/models/colony_state.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace anaiero::session {
struct EngineView {
  engine::Snapshot working{},published{};
  engine::Kind kind=engine::Kind::sw5;
  engine::Capabilities capabilities{};
  std::uint32_t source=0;
  std::uint64_t coordinator_revision=0;
  std::uint8_t cadence=1;
  bool route_active=false,publication_pending=false;
  std::string model_sha256,classification;
};
struct StorageView {
  bool known=false,sustained=false,writable=false;
  std::uint64_t metadata_events=0,evidence_events=0,available_bytes=0,archived_segments=0;
  std::size_t metadata_resident=0,evidence_resident=0,segment_events=0;
};
struct ExperimentView {
  std::string id,definition_sha256,source_sha256,model_sha256,behavior;
  std::uint32_t engine=0,source=0;
  std::uint64_t epoch=0,start_tick=0,seed=0;
  experiments::RunState state=experiments::RunState::prepared;
  experiments::Code failure=experiments::Code::ok;
  std::size_t count=0,completed=0;
  std::array<experiments::TrialReceipt,experiments::kMaxCandidates> trials{};
  engine::Selection selection{};
  experiments::Outcome outcome{};
  bool policy_paused=false;
};
struct View {
  std::string name,error;
  std::uint64_t revision=0,cycles=0,target_cycles=0,operation_id=0;
  std::size_t pending_evidence=0,engine_count=0;
  bool busy=false;
  std::array<EngineView,engine::kMaxEngines> engines{};
  runtime::ColonyState projected{};
  StorageView storage{};
  std::uint32_t proposal_engine=0,proposal_candidate=0;
  bool proposal_approved=false;
  std::int64_t proposal_score=0;
  std::uint64_t proposal_commit=0;
  std::string proposal_source;
  std::string experiment_id,experiment_state;
  std::uint32_t completed_trials=0,total_trials=0;
};
struct IViewEncoder {
  virtual ~IViewEncoder()=default;
  virtual std::string encode(const View&)const=0;
};
} // namespace anaiero::session
