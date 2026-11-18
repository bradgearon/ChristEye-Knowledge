#include "anaiero/sdk/native_session.hpp"
#include "anaiero/sdk/engine_files.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iostream>
#include <mutex>
#include <source_location>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <spawn.h>
#include <sys/wait.h>
extern char** environ;
#endif

namespace s=anaiero::sdk;
namespace h=s::host;
namespace e=anaiero::engines;
namespace ev=s::evidence;
namespace fs=std::filesystem;
using namespace std::chrono_literals;
#define CHECK(condition) do {if(!(condition)){std::cerr<<"FAILED line "<<__LINE__<<": " #condition "\n";std::exit(1);}}while(false)
void require(const s::Result& result,std::source_location where=std::source_location::current()) {
  if(!result) {std::cerr<<"FAILED line "<<where.line()<<": "<<s::code_name(result.code)<<": "<<result.message<<'\n';std::exit(1);}
}
void initialize(const fs::path& repository,const fs::path& directory) {
  fs::remove_all(directory);s::EngineModuleCatalog catalog;
  require(e::register_builtin_engines(catalog));require(catalog.freeze());
  s::SessionDefinition definition;
  require(e::read_session_definition((repository/"examples/runtime/modular-native.cevsession").string(),catalog,definition));
  const auto environment=definition.engines.back();definition.engines={environment};
  std::unique_ptr<ev::ICommitStore> store;require(ev::open_native_store(directory.string(),{},store));
  s::ProjectionService projection;s::SessionService runtime(catalog,*store,projection);
  s::SessionRunner runner(runtime,*store);require(runner.initialize(definition));
}
template<class Predicate>h::Status await_status(h::NativeContinuousHost& host,Predicate predicate) {
  const auto deadline=std::chrono::steady_clock::now()+30s;
  auto status=host.snapshot();
  while(!predicate(status)&&std::chrono::steady_clock::now()<deadline)status=host.snapshot(status.revision,1000);
  CHECK(predicate(status));return status;
}
void continuous(const fs::path& directory) {
  h::NativeSession session;require(session.open(directory.string()));require(session.ensure_engines());
  h::Options options;options.initially_running=false;options.period_ms=1;options.command_capacity=2;options.task_capacity=2;
  h::NativeContinuousHost host(session.runtime(),session.runner(),session.store(),options);
  std::mutex watermark_mutex;std::vector<std::uint64_t> watermarks;
  host.set_commit_wakeup([&](std::uint64_t value){std::lock_guard lock(watermark_mutex);watermarks.push_back(value);});
  require(host.start());
  const auto initial=await_status(host,[](const auto& value){return value.revision>0;});
  const auto quiet=host.snapshot(initial.revision,80);
  CHECK(quiet.revision==initial.revision&&quiet.owner_wakeups==initial.owner_wakeups);
  std::uint64_t target_ticket=0;
  require(host.enqueue({h::CommandKind::through,{},{},3},target_ticket));
  const auto finished=await_status(host,[](const auto& value){return value.completed_cycles==3&&!value.cycle_pending;});
  CHECK(finished.engines.size()==1&&finished.engines.front().tick==3&&finished.result.ok());
  h::CommandResult target;require(host.command_result(target_ticket,target));
  CHECK(target.complete&&!target.durable&&target.result.ok());
  std::uint64_t pause_ticket=0;
  require(host.enqueue({h::CommandKind::engine_pause,"environment","pause-stable",0},pause_ticket));
  await_status(host,[](const auto& value){return value.engines.front().phase==s::Phase::paused;});
  h::CommandResult paused;require(host.command_result(pause_ticket,paused));CHECK(paused.complete&&paused.durable);
  // Retrying the same stable engine operation cannot increment its revision.
  const auto revision=host.snapshot().engines.front().revision;
  std::uint64_t retry_ticket=0;require(host.enqueue({h::CommandKind::engine_pause,"environment","pause-stable",0},retry_ticket));
  await_status(host,[&](const auto&){h::CommandResult value;require(host.command_result(retry_ticket,value));return value.complete;});
  CHECK(host.snapshot().engines.front().revision==revision);

  std::mutex gate_mutex;std::condition_variable gate;bool released=false;
  std::promise<void> entered,first_done,second_done;
  auto entered_future=entered.get_future(),first_future=first_done.get_future(),second_future=second_done.get_future();
  require(host.submit([&](ev::ICommitStore& store){
    entered.set_value();{std::unique_lock lock(gate_mutex);gate.wait(lock,[&]{return released;});}
    ev::CommitBatch batch;batch.operation_id="host-external-1";batch.request_digest=s::sha256("host-external-1");
    batch.records.push_back({"host.test","one","committed",{},false});ev::CommitReceipt receipt;return store.commit(batch,receipt);
  },[&](s::Result result){require(result);first_done.set_value();}));
  CHECK(entered_future.wait_for(5s)==std::future_status::ready);
  require(host.submit([](ev::ICommitStore& store){
    ev::CommitBatch batch;batch.operation_id="host-external-2";batch.request_digest=s::sha256("host-external-2");
    batch.records.push_back({"host.test","two","committed",{},false});ev::CommitReceipt receipt;return store.commit(batch,receipt);
  },[&](s::Result result){require(result);second_done.set_value();}));
  CHECK(host.submit([](auto&){return s::Result{};},[](auto){}).code==s::Code::capacity);
  std::uint64_t one=0,two=0,three=0;
  require(host.enqueue({h::CommandKind::run,{},{},0},one));require(host.enqueue({h::CommandKind::pause,{},{},0},two));
  CHECK(host.enqueue({h::CommandKind::pause,{},{},0},three).code==s::Code::capacity);
  const auto cached=host.snapshot();CHECK(cached.busy&&cached.queued_tasks==1&&cached.queued_commands==2);
  CHECK(h::encode_status(cached).size()<16384);
  host.request_stop();CHECK(host.submit([](auto&){return s::Result{};},[](auto){}).code==s::Code::busy);
  {std::lock_guard lock(gate_mutex);released=true;}gate.notify_one();
  CHECK(first_future.wait_for(5s)==std::future_status::ready);
  CHECK(second_future.wait_for(5s)==std::future_status::ready);
  host.shutdown();const auto stopped=host.snapshot();CHECK(stopped.stopped&&!stopped.cycle_pending&&stopped.completed_tasks==2);
  ev::StoredRecord retained;require(session.store().latest("host.test","two",retained));
  CHECK(retained.record.payload=="committed");
  for(std::size_t i=1;i<watermarks.size();++i)CHECK(watermarks[i]>watermarks[i-1]);
}
void pending_recovery(const fs::path& directory) {
  std::uint64_t generation=0;
  {
    h::NativeSession session;require(session.open(directory.string()));require(session.ensure_engines());
    require(session.runtime().control("environment","resume-stable",s::Phase::running));
    require(session.runner().begin_cycle());
    std::string id;s::StepRequest request;bool pending=false;require(session.runner().next_step(id,request,pending));CHECK(pending);
    require(session.runtime().step(id,request));
    generation=session.store().generation();
    CHECK(session.runner().progress().cycle_pending);
  }
  h::NativeSession recovered;require(recovered.open(directory.string()));require(recovered.ensure_engines());
  CHECK(recovered.store().generation()==generation&&recovered.runner().progress().cycle_pending);
  h::Options options;options.initially_running=false;
  h::NativeContinuousHost host(recovered.runtime(),recovered.runner(),recovered.store(),options);require(host.start());
  const auto settled=await_status(host,[](const auto& value){return value.completed_cycles==4&&!value.cycle_pending;});
  CHECK(settled.engines.front().tick==4);host.shutdown();
  ev::AuditReport audit;require(recovered.store().audit(audit));
}
struct CrashObserver final:s::IObserverSink {
  s::Result enqueue(const s::ObserverEvent& event) override {
    if(event.kind==s::ObserverEventKind::committed&&event.tick==1)std::_Exit(71);
    return {};
  }
};
int crash_child(const fs::path& directory) {
  h::NativeSession session;require(session.open(directory.string()));require(session.ensure_engines());
  CrashObserver observer;session.runtime().set_observer_sink(&observer);
  h::Options options;options.period_ms=1;options.initially_running=false;
  h::NativeContinuousHost host(session.runtime(),session.runner(),session.store(),options);require(host.start());
  std::uint64_t ticket=0;require(host.enqueue({h::CommandKind::through,{},{},1},ticket));
  await_status(host,[](const auto& value){return value.completed_cycles==1;});return 72;
}
void process_cut(const fs::path& executable,const fs::path& directory) {
#ifdef _WIN32
  const auto quote=[](std::wstring_view text){
    std::wstring out=L"\"";std::size_t slashes=0;
    for(const auto c:text) {if(c==L'\\') {++slashes;continue;}
      out.append(c==L'"'?slashes*2+1:slashes,L'\\');out+=c;slashes=0;}
    out.append(slashes*2,L'\\');out+=L'"';return out;
  };
  const auto exe=fs::absolute(executable).wstring();
  auto command=quote(exe)+L" --crash-after-commit "+quote(fs::absolute(directory).wstring());
  STARTUPINFOW startup{};startup.cb=static_cast<DWORD>(sizeof(startup));PROCESS_INFORMATION child{};
  CHECK(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,nullptr,&startup,&child));
  CloseHandle(child.hThread);CHECK(WaitForSingleObject(child.hProcess,60000)==WAIT_OBJECT_0);
  DWORD code=0;CHECK(GetExitCodeProcess(child.hProcess,&code));CloseHandle(child.hProcess);CHECK(code==71);
#else
  std::vector<std::string> owned{fs::absolute(executable).string(),"--crash-after-commit",fs::absolute(directory).string()};
  std::vector<char*> args;for(auto& item:owned)args.push_back(item.data());args.push_back(nullptr);
  pid_t child=-1;CHECK(posix_spawn(&child,args.front(),nullptr,nullptr,args.data(),environ)==0);
  int code=0;pid_t done=-1;do{done=waitpid(child,&code,0);}while(done<0&&errno==EINTR);
  CHECK(done==child&&WIFEXITED(code)&&WEXITSTATUS(code)==71);
#endif
  h::NativeSession session;require(session.open(directory.string()));require(session.ensure_engines());
  CHECK(session.runner().progress().completed_cycles==0&&session.runner().progress().cycle_pending);
  CHECK(session.runtime().find("environment")->state().tick==1);
  h::Options options;options.initially_running=false;
  h::NativeContinuousHost host(session.runtime(),session.runner(),session.store(),options);require(host.start());
  const auto recovered=await_status(host,[](const auto& value){return value.completed_cycles==1&&!value.cycle_pending;});
  CHECK(recovered.engines.front().tick==1);host.shutdown();
  ev::AuditReport audit;require(session.store().audit(audit));
}
int main(int argc,char** argv) {
  if(argc==3&&std::string_view(argv[1])=="--crash-after-commit")return crash_child(argv[2]);
  CHECK(argc==3);const fs::path repository=argv[1],directory=argv[2];
  initialize(repository,directory);continuous(directory);pending_recovery(directory);
  const fs::path crash_directory=directory.string()+"-process-cut";
  initialize(repository,crash_directory);process_cut(argv[0],crash_directory);
  std::cout<<"continuous host: bounded queues, event wakes, exact control retry, external lane, drain and process-cut recovery passed\n";
}
