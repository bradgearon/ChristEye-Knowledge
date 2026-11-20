#include "anaiero/sdk/host.hpp"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iomanip>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>

namespace anaiero::sdk::host {
namespace {
using Clock=std::chrono::steady_clock;
std::uint64_t monotonic_ns() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now().time_since_epoch()).count());
}
std::string quoted(std::string_view value) {
  std::ostringstream out;out<<'"';
  for(const unsigned char c:value) {
    if(c=='"'||c=='\\')out<<'\\'<<static_cast<char>(c);
    else if(c<32)out<<"\\u"<<std::hex<<std::setw(4)<<std::setfill('0')<<static_cast<unsigned>(c)<<std::dec;
    else out<<static_cast<char>(c);
  }
  out<<'"';return out.str();
}
bool engine_command(CommandKind kind) {
  return kind==CommandKind::engine_pause||kind==CommandKind::engine_resume||kind==CommandKind::engine_cancel;
}
// Pure serial cycle decisions. Thread, timer and IPC behavior stay in the
// adapter below. Only one prepared engine transition can be outstanding.
class CycleDriver {
  SessionService& runtime_;SessionRunner& runner_;
public:
  CycleDriver(SessionService& runtime,SessionRunner& runner):runtime_(runtime),runner_(runner){}
  Result advance(bool& completed,bool& dispatched) {
    completed=false;dispatched=false;
    if(runtime_.busy())return runtime_.pump();
    if(!runner_.progress().cycle_pending)return runner_.begin_cycle();
    std::string instance;StepRequest request;bool pending=false;
    if(auto result=runner_.next_step(instance,request,pending);!result)return result;
    if(pending){dispatched=true;return runtime_.begin_step(instance,request);}
    const auto result=runner_.finish_cycle();completed=result.ok();return result;
  }
};
}
struct NativeContinuousHost::Impl {
  struct Work {Command command;std::uint64_t ticket=0;};
  struct Task {StoreTask execute;StoreTaskCompletion complete;};
  SessionService& runtime;SessionRunner& runner;evidence::ICommitStore& store;
  Options options;CycleDriver driver;
  mutable std::mutex mutex;mutable std::condition_variable view_changed;
  std::condition_variable wake,task_ready;
  std::deque<Work> commands;std::deque<Task> tasks;
  std::deque<CommandResult> results;
  std::optional<Task> task_slot;
  std::unique_ptr<evidence::CommitWorker> writer;
  struct MeasuredQueue final:evidence::ICommitQueue {
    Impl& owner;
    explicit MeasuredQueue(Impl& value):owner(value){}
    Result submit(evidence::CommitBatch batch,std::uint64_t& ticket) override {
      if(owner.options.timing_observer)owner.submitted_ns=monotonic_ns();
      return owner.writer->submit(std::move(batch),ticket);
    }
    bool try_complete(evidence::CommitCompletion& completion) override {
      if(!owner.writer->try_complete(completion))return false;
      owner.commit_started_ns=completion.started_monotonic_ns;
      owner.commit_completed_ns=completion.completed_monotonic_ns;return true;
    }
  } measured_queue{*this};
  std::thread owner,task_worker;
  Status cached;
  std::uint64_t signal=0,next_ticket=1,watermark=0;
  std::uint64_t through=0,owner_wakeups=0,completed_tasks=0;
  std::uint64_t submitted_ns=0,commit_started_ns=0,commit_completed_ns=0;
  std::uint64_t dispatch_started_ns=0,dispatch_cycle=0;
  bool step_timing_pending=false;
  bool started=false,stopping=false,stopped=false,task_exit=false;
  bool task_active=false,task_done=false,task_threw=false;
  bool running=false,runnable=true;
  Result terminal,task_result;
  std::function<void(std::uint64_t)> commit_wakeup;
  Impl(SessionService& r,SessionRunner& s,evidence::ICommitStore& e,Options o):
    runtime(r),runner(s),store(e),options(o),driver(r,s),running(o.initially_running){}
  void notify() noexcept {
    {std::lock_guard lock(mutex);++signal;}
    wake.notify_one();
  }
  void timing(TimingKind kind,std::uint64_t ns,Code result) noexcept {
    if(options.timing_observer)options.timing_observer->observe({kind,dispatch_cycle,ns,result});
  }
  void publish(bool read_store=true) {
    // The storage lane is quiescent whenever read_store is true. While an
    // engine write is outstanding, only the writer's immutable cache is read.
    auto info=read_store?store.info():writer->cached_info();
    const auto progress=runner.progress();
    std::vector<EngineStatus> previous;
    {std::lock_guard lock(mutex);previous=cached.engines;}
    std::vector<EngineStatus> engines;bool next_runnable=false;
    for(const auto& schedule:runner.definition().engines) {
      const auto* engine=runtime.find(schedule.identity.instance_id);
      if(!engine)continue;
      const auto& state=engine->state();
      const auto found=std::find_if(previous.begin(),previous.end(),[&](const auto& status){
        return status.identity==engine->identity()&&status.revision==state.revision&&status.phase==state.phase&&status.tick==state.tick;
      });
      if(found!=previous.end())engines.push_back(*found);
      else engines.push_back({engine->identity(),state.phase,state.tick,state.revision,state_digest(state)});
      if(read_store&&(state.phase==Phase::ready||state.phase==Phase::running)) {
        bool eligible=false;
        const auto result=runtime.scheduling_admission(schedule.identity.instance_id,eligible);
        if(result&&eligible)next_runnable=true;
        else if(!result&&terminal.ok())terminal=result;
      }
    }
    if(read_store)runnable=next_runnable;
    std::function<void(std::uint64_t)> callback;bool notify_commit=false;
    {
      std::lock_guard lock(mutex);
      if(!read_store&&cached.store.generation>info.generation)info=cached.store;
      cached.store=info;cached.completed_cycles=progress.completed_cycles;
      cached.cycle_pending=progress.cycle_pending;cached.engines=std::move(engines);
      cached.running=running;cached.busy=runtime.busy()||task_active;
      cached.stopping=stopping;cached.stopped=stopped;cached.through_cycle=through;
      cached.queued_commands=commands.size();cached.queued_tasks=tasks.size();
      cached.owner_wakeups=owner_wakeups;cached.completed_tasks=completed_tasks;
      cached.result=terminal;++cached.revision;
      if(info.generation>watermark) {watermark=info.generation;callback=commit_wakeup;notify_commit=true;}
    }
    view_changed.notify_all();
    if(notify_commit&&callback)try{callback(info.generation);}catch(...){}
  }
  void finish_command(const Work& work,Result result,bool durable) {
    std::lock_guard lock(mutex);
    for(auto& value:results)if(value.ticket==work.ticket) {
      value.complete=true;value.durable=durable;value.result=std::move(result);return;
    }
  }
  void execute_command(const Work& work) {
    Result result;bool durable=false;
    if(work.command.kind==CommandKind::pause) {running=false;through=0;}
    else if(work.command.kind==CommandKind::run) {running=true;through=0;}
    else if(work.command.kind==CommandKind::through) {
      const auto current=runner.progress().completed_cycles;
      if(work.command.through_cycle<current)result={Code::conflict,"target precedes completed cycles"};
      else if(work.command.through_cycle-current>10000)result={Code::capacity,"one target admits at most 10000 cycles"};
      else {through=work.command.through_cycle;running=false;}
    } else {
      const auto phase=work.command.kind==CommandKind::engine_pause?Phase::paused:
        work.command.kind==CommandKind::engine_resume?Phase::running:Phase::cancelled;
      result=runtime.control(work.command.instance_id,work.command.operation_id,phase);
      durable=result.committed();
    }
    finish_command(work,result,durable);
    if(result.code==Code::committed_degraded)terminal=result;
  }
  void task_loop() noexcept {
    for(;;) {
      Task task;
      {
        std::unique_lock lock(mutex);
        task_ready.wait(lock,[&]{return task_exit||task_slot.has_value();});
        if(task_exit&&!task_slot)return;
        task=std::move(*task_slot);task_slot.reset();
      }
      Result result;bool threw=false;
      try {result=task.execute(store);}
      catch(...) {result={Code::io_error,"storage task threw; reconcile its canonical operation before retry"};threw=true;}
      // Completion has reserved capacity: it cannot be lost behind a full
      // request queue. It runs outside host locks and cannot reenter the store.
      if(task.complete)try{task.complete(result);}catch(...){}
      {
        std::lock_guard lock(mutex);task_done=true;task_threw=threw;task_result=result;++signal;
      }
      wake.notify_one();
    }
  }
  void loop() noexcept {
    auto next=Clock::now()+std::chrono::milliseconds(options.period_ms);
    bool prefer_task=true;
    try {
      publish();
      for(;;) {
        std::uint64_t observed=0;bool stop=false,active=false,done=false,threw=false;Result completed_result;
        {
          std::lock_guard lock(mutex);observed=signal;stop=stopping;active=task_active;
          done=task_done;threw=task_threw;completed_result=task_result;
          if(done) {task_active=false;task_done=false;task_threw=false;}
        }
        if(done) {
          ++completed_tasks;prefer_task=false;
          if(threw)terminal={Code::io_error,"storage task threw; host requires explicit recovery"};
          else if(completed_result.code==Code::committed_degraded)terminal=completed_result;
          else if(!store.info().writable)terminal={Code::io_error,"canonical storage stopped admission; explicit recovery is required"};
          publish();continue;
        }
        if(!active&&runtime.busy()) {
          const auto projection_started=options.timing_observer?monotonic_ns():0;
          const auto result=runtime.pump();
          if(result.code==Code::busy) {
            std::unique_lock lock(mutex);wake.wait(lock,[&]{return signal!=observed;});++owner_wakeups;
          }else {
            if(step_timing_pending){
              if(commit_started_ns>=submitted_ns)timing(TimingKind::step_queue_wait,commit_started_ns-submitted_ns,result.code);
              if(commit_completed_ns>=commit_started_ns)timing(TimingKind::step_commit_completion,commit_completed_ns-commit_started_ns,result.code);
              timing(TimingKind::step_projection,monotonic_ns()-projection_started,result.code);
              step_timing_pending=false;
            }
            if(!result)terminal=result;
            prefer_task=true;publish(!runtime.busy());
          }
          continue;
        }
        if(!active) {
          Work work;bool have_work=false;
          {
            std::lock_guard lock(mutex);
            if(!commands.empty()&&(stop||!terminal.ok()||!runner.progress().cycle_pending||!engine_command(commands.front().command.kind))) {
              work=std::move(commands.front());commands.pop_front();have_work=true;
            }
          }
          if(have_work) {
            if(stop)finish_command(work,{Code::cancelled,"host is stopping; queued control was not applied"},false);
            else if(!terminal)finish_command(work,{Code::busy,"host requires recovery"},false);
            else execute_command(work);
            publish();continue;
          }
          const bool pending=runner.progress().cycle_pending;
          const bool target=through>runner.progress().completed_cycles;
          const bool due=terminal.ok()&&!stop&&((running&&runnable)||target)&&Clock::now()>=next;
          {
            std::lock_guard lock(mutex);
            if(!tasks.empty()&&(prefer_task||(!pending&&!due))) {
              task_slot=std::move(tasks.front());tasks.pop_front();task_active=true;
              ++cached.revision;cached.busy=true;cached.queued_tasks=tasks.size();active=true;
            }
          }
          if(active) {view_changed.notify_all();task_ready.notify_one();continue;}
          if(terminal.ok()&&(pending||due)) {
            submitted_ns=0;commit_started_ns=0;commit_completed_ns=0;
            if(options.timing_observer){dispatch_started_ns=monotonic_ns();dispatch_cycle=runner.progress().completed_cycles+1;}
            bool completed=false,dispatched=false;const auto result=driver.advance(completed,dispatched);
            if(dispatched&&options.timing_observer){
              timing(TimingKind::step_dispatch,monotonic_ns()-dispatch_started_ns,result.code);
              step_timing_pending=runtime.busy();
            }
            if(!result&&result.code!=Code::busy)terminal=result;
            if(completed)next=Clock::now()+std::chrono::milliseconds(options.period_ms);
            prefer_task=true;publish(!runtime.busy());continue;
          }
          bool drained=false;
          {
            std::lock_guard lock(mutex);
            // A failed cycle remains durably resumable. Stop never fabricates
            // completion; recovery can finish the exact outstanding intents.
            drained=stop&&tasks.empty()&&commands.empty()&&(!pending||!terminal.ok());
          }
          if(drained)break;
        }
        std::unique_lock lock(mutex);
        const bool timed=!active&&terminal.ok()&&!stopping&&
          ((running&&runnable)||through>runner.progress().completed_cycles);
        if(timed)wake.wait_until(lock,next,[&]{return signal!=observed;});
        else wake.wait(lock,[&]{return signal!=observed;});
        ++owner_wakeups;
      }
    }catch(...) {terminal={Code::io_error,"native host owner failed; canonical recovery required"};}
    writer->stop();runtime.set_commit_queue(nullptr);
    std::deque<Task> abandoned;
    {
      std::lock_guard lock(mutex);task_exit=true;stopped=true;running=false;
      abandoned.swap(tasks);
      for(auto& result:results)if(!result.complete) {
        result.complete=true;result.result={Code::cancelled,"host stopped before this queued control"};
      }
    }
    for(auto& task:abandoned)if(task.complete)try{task.complete({Code::cancelled,"host stopped before this storage task"});}catch(...){}
    task_ready.notify_all();
    try{publish();}catch(...){}
    view_changed.notify_all();
  }
};
NativeContinuousHost::NativeContinuousHost(SessionService& runtime,SessionRunner& runner,
    evidence::ICommitStore& store,Options options):impl_(std::make_unique<Impl>(runtime,runner,store,options)){}
NativeContinuousHost::~NativeContinuousHost(){shutdown();}
Result NativeContinuousHost::start() {
  auto& p=*impl_;
  {
    std::lock_guard lock(p.mutex);
    if(p.started)return {Code::conflict,"host is already started"};
    if(p.options.period_ms==0||p.options.period_ms>3600000||p.options.command_capacity==0||
       p.options.command_capacity>256||p.options.task_capacity==0||p.options.task_capacity>256)
      return {Code::invalid,"host period or queue capacity is outside its declared bound"};
    if(p.runtime.busy())return {Code::busy,"runtime has an outstanding owner"};
    p.started=true;
  }
  try {
    p.writer=std::make_unique<evidence::CommitWorker>(p.store,1);
    p.writer->set_wakeup([&p]{p.notify();});p.runtime.set_commit_queue(&p.measured_queue);
    p.task_worker=std::thread([&p]{p.task_loop();});
    p.owner=std::thread([&p]{p.loop();});
  }catch(...) {
    {std::lock_guard lock(p.mutex);p.stopping=true;p.stopped=true;p.task_exit=true;}
    p.task_ready.notify_all();
    if(p.task_worker.joinable())p.task_worker.join();
    if(p.writer)p.writer->stop();
    p.runtime.set_commit_queue(nullptr);
    return {Code::io_error,"cannot start native host threads"};
  }
  return {};
}
Result NativeContinuousHost::enqueue(Command command,std::uint64_t& ticket) {
  auto& p=*impl_;ticket=0;
  if(static_cast<std::uint32_t>(command.kind)>static_cast<std::uint32_t>(CommandKind::engine_cancel))
    return {Code::invalid,"unknown host command"};
  if(engine_command(command.kind)&&(!valid_token(command.instance_id)||!valid_token(command.operation_id)))
    return {Code::invalid,"engine controls require bounded instance and stable operation identities"};
  {
    std::lock_guard lock(p.mutex);
    if(!p.started||p.stopping||p.stopped)return {Code::busy,"host is not accepting controls"};
    if(p.commands.size()>=p.options.command_capacity||p.next_ticket==std::numeric_limits<std::uint64_t>::max())
      return {Code::capacity,"host command queue is full"};
    ticket=p.next_ticket++;p.commands.push_back({std::move(command),ticket});
    p.results.push_back({ticket,false,false,{}});
    while(p.results.size()>p.options.command_capacity+32&&p.results.front().complete)p.results.pop_front();
    p.cached.queued_commands=p.commands.size();++p.cached.revision;++p.signal;
  }
  p.view_changed.notify_all();p.wake.notify_one();return {};
}
Result NativeContinuousHost::submit(StoreTask task,StoreTaskCompletion completion) {
  if(!task||!completion)return {Code::invalid,"storage work requires owned work and a completion"};
  auto& p=*impl_;
  {
    std::lock_guard lock(p.mutex);
    if(!p.started||p.stopping||p.stopped)return {Code::busy,"host is not accepting storage work"};
    if(!p.cached.result.ok())return {Code::busy,"host requires explicit recovery before further storage work"};
    if(p.tasks.size()+(p.task_active?1U:0U)>=p.options.task_capacity)
      return {Code::capacity,"host storage task queue is full"};
    p.tasks.push_back({std::move(task),std::move(completion)});
    p.cached.queued_tasks=p.tasks.size();++p.cached.revision;++p.signal;
  }
  p.view_changed.notify_all();p.wake.notify_one();return {};
}
Status NativeContinuousHost::snapshot(std::uint64_t after,std::uint32_t wait_ms) const {
  auto& p=*impl_;std::unique_lock lock(p.mutex);
  if(wait_ms&&p.cached.revision<=after)
    p.view_changed.wait_for(lock,std::chrono::milliseconds(std::min(wait_ms,30000U)),[&]{return p.cached.revision>after||p.stopped;});
  return p.cached;
}
Result NativeContinuousHost::command_result(std::uint64_t ticket,CommandResult& output) const {
  auto& p=*impl_;std::lock_guard lock(p.mutex);
  for(const auto& result:p.results)if(result.ticket==ticket) {output=result;return {};}
  return {ticket&&ticket<p.next_ticket?Code::expired:Code::not_found,
    "host ticket is absent; durable engine controls are reconciled by their operation ID"};
}
void NativeContinuousHost::set_commit_wakeup(std::function<void(std::uint64_t)> callback) {
  std::lock_guard lock(impl_->mutex);impl_->commit_wakeup=std::move(callback);
}
void NativeContinuousHost::request_stop() noexcept {
  auto& p=*impl_;
  {std::lock_guard lock(p.mutex);p.stopping=true;++p.signal;++p.cached.revision;p.cached.stopping=true;}
  p.wake.notify_one();p.view_changed.notify_all();
}
void NativeContinuousHost::shutdown() noexcept {
  request_stop();
  if(impl_->owner.joinable())impl_->owner.join();
  if(impl_->task_worker.joinable())impl_->task_worker.join();
}
Bytes encode_status(const Status& value,std::size_t offset,std::size_t limit) {
  limit=std::min<std::size_t>(limit,8);
  std::ostringstream out;out<<"{\"schema\":\"anaiero.host-status/1\",\"view_revision\":"<<value.revision
    <<",\"running\":"<<(value.running?"true":"false")<<",\"busy\":"<<(value.busy?"true":"false")
    <<",\"stopping\":"<<(value.stopping?"true":"false")<<",\"stopped\":"<<(value.stopped?"true":"false")
    <<",\"completed_cycles\":"<<value.completed_cycles<<",\"cycle_pending\":"<<(value.cycle_pending?"true":"false")
    <<",\"through_cycle\":"<<value.through_cycle<<",\"owner_wakeups\":"<<value.owner_wakeups
    <<",\"completed_tasks\":"<<value.completed_tasks<<",\"queued_commands\":"<<value.queued_commands
    <<",\"queued_tasks\":"<<value.queued_tasks<<",\"generation\":"<<value.store.generation
    <<",\"accounted_bytes\":"<<value.store.physical_bytes<<",\"maximum_bytes\":"<<value.store.maximum_bytes
    <<",\"quota_basis\":\"accounted_serialized_bytes\",\"code\":"<<quoted(code_name(value.result.code))
    <<",\"message\":"<<quoted(value.result.message)<<",\"engines\":[";
  const auto end=offset<value.engines.size()?offset+std::min(limit,value.engines.size()-offset):offset;
  for(auto i=offset;i<end;++i) {
    const auto& engine=value.engines[i];if(i!=offset)out<<',';
    out<<"{\"id\":"<<quoted(engine.identity.instance_id)<<",\"source\":"<<quoted(engine.identity.source_uid)
      <<",\"epoch\":"<<engine.identity.epoch<<",\"phase\":"<<quoted(phase_name(engine.phase))
      <<",\"tick\":"<<engine.tick<<",\"revision\":"<<engine.revision<<",\"state_sha256\":"<<quoted(engine.state_sha256)<<'}';
  }
  out<<"],\"next_engine\":"<<(end<value.engines.size()?std::to_string(end):"null")<<"}\n";return out.str();
}
}
