#pragma once
// Passive session configuration, recovery records and injected persistence ports.
#include "anaiero/contracts/simulation_engine.hpp"
#include "anaiero/contracts/engine_optimization.hpp"
#include "anaiero/contracts/fork_optimization.hpp"
#include "anaiero/contracts/cell_behavior.hpp"
#include "anaiero/contracts/operation.hpp"
#include "anaiero/contracts/session_view.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
namespace anaiero::session {
struct EngineConfig {
  std::uint32_t id=0,source=0;
  std::uint64_t epoch=1;
  engine::Kind kind=engine::Kind::sw5;
  std::uint8_t cadence=1;
  std::string model_path,initial_path;
  engine::Inputs inputs{};
  std::string solver_configuration_sha256{};
};
struct Configuration {
  std::string name,digest;
  std::uint32_t period_ms=100;
  bool sustained_storage=false;
  std::uint32_t segment_events=256;
  std::size_t count=0;
  std::array<EngineConfig,engine::kMaxEngines> engines{};
};
struct RouteState {
  bool active=false,prior_existed=false;
  std::uint16_t prior_guidance=0;
  std::uint64_t change_id=0;
  optimization::Candidate selected{};
};
struct EngineEvidence {
  std::uint64_t next_sequence=1;
  engine::Snapshot published{};
  RouteState route{};
};
struct Publication {
  bool active=false;
  std::size_t engine_index=0,metric_index=0;
  engine::Snapshot snapshot{};
};
struct PendingChange {
  bool active=false;
  std::size_t engine_index=0;
  std::string source_sha256,target_checkpoint;
};
struct Proposal {
  std::uint32_t engine_id=0;
  std::uint16_t horizon=0;
  optimization::ForkOption option{};
  engine::Selection selection{};
};
struct EngineContract {
  std::string behavior;
  std::uint32_t input_schema=0,metric_schema=0,checkpoint_schema=0,clock_domain=0;
  std::uint64_t tick_ns=0;
  std::array<engine::SummarySemantic,4> summaries{};
  bool isolated_fork=false,evaluate_candidates=false,apply_changes=false,restore=false;
  bool operator==(const EngineContract&)const=default;
};
struct Record {
  std::uint32_t schema=2;
  std::uint64_t revision=0,changes=0,target_cycles=0;
  std::uint64_t operation_id=0;
  std::uint64_t experiment_operation_id=0;
  bool experiment_pending=false;
  bool operation_cancelled=false;
  bool experiment_policy_paused=false;
  std::string configuration_sha256;
  engine::CoordinatorProgress coordinator{};
  std::array<EngineEvidence,engine::kMaxEngines> evidence{};
  std::array<EngineContract,engine::kMaxEngines> contracts{};
  Publication publication{};
  PendingChange change{};
  Proposal proposal{};
};
struct IRecordStore {
  virtual ~IRecordStore()=default;
  virtual bool load(Record&,bool& exists) noexcept=0;
  // One durable manifest revision; increments revision only on success.
  virtual bool save(Record&) noexcept=0;
};
struct IEngineCheckpointStore {
  virtual ~IEngineCheckpointStore()=default;
  virtual bool save(const engine::Descriptor&,const engine::Snapshot&,
                    std::string_view checkpoint) noexcept=0;
};
struct ISw5Port {
  virtual ~ISw5Port()=default;
  virtual const cells::Atlas& atlas()const noexcept=0;
  virtual cells::ISpatialPort& spatial()noexcept=0;
  virtual bool world(cells::World&)const noexcept=0;
  virtual bool encode(const cells::World&,std::string&)const noexcept=0;
  virtual bool replace(std::string_view checkpoint)noexcept=0;
};
struct ICancellation {
  virtual ~ICancellation()=default;
  virtual bool cancelled()const noexcept=0;
};
struct IWorkBudget {
  virtual ~IWorkBudget()=default;
  virtual bool admits(std::size_t engines)const noexcept=0;
  virtual StorageView storage()const noexcept{return {};}
};
enum class Operation : std::uint8_t {step,analyze,apply,revert,retry,experiment,resume_experiment};
struct Command {
  Operation operation=Operation::step;
  std::uint32_t engine=0,from=0,to=0,count=1;
  std::uint64_t operation_id=0;
  std::string reference{};
};
struct IDriver {
  virtual ~IDriver()=default;
  virtual bool execute(const Command&,const ICancellation*)noexcept=0;
  virtual bool pump()noexcept=0;
  virtual bool busy()const noexcept=0;
  virtual bool quiescent()const noexcept=0;
  virtual bool failed()const noexcept=0;
  virtual std::string state_json()const=0;
  virtual std::uint64_t revision()const noexcept=0;
  virtual std::uint64_t operation_id()const noexcept{return 0;}
  virtual View view()const{return {};}
  virtual ExperimentView experiment_view()const{return {};}
  virtual operations::View operation_view()const{return {};}
  virtual void cancel_operations()noexcept{}
  // Thread-safe signal only. Implementations must not mutate owner state here.
  virtual void request_cancel()noexcept{}
  virtual std::string trace_json()const{return "{\"events\":[]}\n";}
};
struct IEngineCancellation {
  virtual ~IEngineCancellation()=default;
  virtual void request_cancel()noexcept=0;
};
} // namespace anaiero::session
