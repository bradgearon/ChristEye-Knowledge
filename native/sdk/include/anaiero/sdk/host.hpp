#pragma once
#include "anaiero/sdk/session_runner.hpp"
#include <functional>
#include <memory>

namespace anaiero::sdk::host {
using StoreTask=std::function<Result(evidence::ICommitStore&)>;
using StoreTaskCompletion=std::function<void(Result)>;
// Trusted adapters submit bounded work, never arbitrary remote code. Accepted
// work owns its captures and completes exactly once. Success here means queued,
// not durable; only the completion carries the canonical operation result.
// Completion signals its consumer on the task worker; it must not wait for
// another host operation, call shutdown, or access the store again.
class IStoreTaskExecutor {
public:
  virtual ~IStoreTaskExecutor()=default;
  virtual Result submit(StoreTask,StoreTaskCompletion)=0;
};
struct Options {
  // Wall monotonic pacing is independent of each engine's logical clock.
  std::uint32_t period_ms=100;
  std::size_t command_capacity=32,task_capacity=16;
  bool initially_running=true;
};
enum class CommandKind : std::uint32_t {run,pause,through,engine_pause,engine_resume,engine_cancel};
struct Command {
  CommandKind kind=CommandKind::pause;
  std::string instance_id,operation_id;
  std::uint64_t through_cycle=0;
};
struct CommandResult {
  std::uint64_t ticket=0;
  bool complete=false,durable=false;
  Result result;
};
struct EngineStatus {
  EngineIdentity identity;
  Phase phase=Phase::ready;
  std::uint64_t tick=0,revision=0;
  Digest state_sha256;
};
struct Status {
  std::uint64_t revision=0,completed_cycles=0,through_cycle=0;
  std::uint64_t owner_wakeups=0,completed_tasks=0;
  std::size_t queued_commands=0,queued_tasks=0;
  bool running=false,busy=false,cycle_pending=false,stopping=false,stopped=false;
  Result result;
  evidence::StoreInfo store;
  std::vector<EngineStatus> engines;
};
// Native threading adapter around one SessionService/SessionRunner owner.
// Borrowed services must outlive this object and may not be called concurrently.
// Storage tasks are serialized with engine commits, on a separate bounded lane.
// Cached status/control admission remain available while either lane is busy.
class NativeContinuousHost final:public IStoreTaskExecutor {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  NativeContinuousHost(SessionService&,SessionRunner&,evidence::ICommitStore&,Options={});
  ~NativeContinuousHost();
  NativeContinuousHost(const NativeContinuousHost&)=delete;
  NativeContinuousHost& operator=(const NativeContinuousHost&)=delete;
  Result start();
  Result enqueue(Command,std::uint64_t& ticket);
  Result submit(StoreTask,StoreTaskCompletion) override;
  Status snapshot(std::uint64_t after_revision=0,std::uint32_t wait_ms=0) const;
  Result command_result(std::uint64_t ticket,CommandResult&) const;
  // Signal-only callback, never reenter the host or touch the borrowed store.
  void set_commit_wakeup(std::function<void(std::uint64_t)>);
  void request_stop() noexcept;
  void shutdown() noexcept;
};
// Bounded IPC presentation; engine checkpoints and payloads are never copied
// into this cached response. Source/engine details can be paginated by offset.
Bytes encode_status(const Status&,std::size_t engine_offset=0,std::size_t engine_limit=8);
}
