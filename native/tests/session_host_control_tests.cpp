// No sockets: exercise the worker/control boundary with a deliberately blocked
// driver, deterministic cancellation gates and a recorded future-work target.
#include "anaiero/infrastructure/session_host_worker.hpp"
#include "anaiero/services/session_command_service.hpp"
#include <atomic>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#define CHECK(e) do {if(!(e)){std::cerr<<"FAILED " #e <<" line "<<__LINE__<<"\n";std::abort();}}while(false)
namespace session=anaiero::session;
namespace host=anaiero::host;
using namespace std::chrono_literals;

template<class Predicate>
bool eventually(Predicate predicate,std::chrono::milliseconds timeout=2000ms){
  const auto until=std::chrono::steady_clock::now()+timeout;
  while(!predicate()&&std::chrono::steady_clock::now()<until)
    std::this_thread::sleep_for(1ms);
  return predicate();
}
std::string text(const host::Reply& reply){
  CHECK(reply.length<=reply.bytes.size());
  return {reply.bytes.data(),reply.length};
}
std::uint64_t view_revision(const std::string& json){
  constexpr std::string_view key="\"view_revision\":";
  const auto at=json.find(key);CHECK(at!=std::string::npos);
  const auto* first=json.data()+at+key.size();
  std::uint64_t result=0;
  const auto [end,code]=std::from_chars(first,json.data()+json.size(),result);
  CHECK(code==std::errc{}&&end>first);return result;
}
class FakeDriver final:public session::IDriver {
  mutable std::mutex gate_mutex_;
  std::condition_variable gate_;
  bool released_=false;
  std::uint32_t active_pumps_=0;
public:
  std::atomic<bool> block{false},entered{false},saw_cancel{false};
  std::atomic<bool> record_targets{false},allow_pump{false},active{false};
  std::atomic<std::uint64_t> calls{0},completed{0},target{0},revision_value{0};
  void release(){
    {std::lock_guard<std::mutex> lock(gate_mutex_);released_=true;}
    gate_.notify_all();
  }
  bool execute(const session::Command& command,const session::ICancellation* cancellation)noexcept override{
    ++calls;
    if(block.load()){
      std::unique_lock<std::mutex> lock(gate_mutex_);
      entered.store(true);
      gate_.wait(lock,[&]{return released_;});
    }
    if(cancellation&&cancellation->cancelled()){
      saw_cancel.store(true);return false;
    }
    if(command.operation==session::Operation::step){
      if(record_targets.load()){
        target.fetch_add(command.count);active_pumps_=2;active.store(true);
      }else completed.fetch_add(command.count);
    }
    ++revision_value;return true;
  }
  bool pump()noexcept override{
    if(!record_targets.load()||!allow_pump.load())return false;
    if(!active.load()){
      if(completed.load()==target.load())return false;
      active.store(true);active_pumps_=2;
    }
    if(--active_pumps_==0){++completed;active.store(false);}
    ++revision_value;return true;
  }
  bool busy()const noexcept override{
    return record_targets.load()&&(active.load()||completed.load()<target.load());
  }
  bool quiescent()const noexcept override{return !active.load();}
  bool failed()const noexcept override{return false;}
  std::string state_json()const override{
    return "{\"completed\":"+std::to_string(completed.load())+
      ",\"target\":"+std::to_string(target.load())+"}";
  }
  std::uint64_t revision()const noexcept override{return revision_value.load();}
};
struct RecordingControl final:session::IControl {
  session::Command last{};
  std::uint64_t enqueued=0,after=0;
  std::uint32_t wait=0;
  bool running=false,cancelled=false,stopping=false,accept=true;
  std::string state="{}\n";
  bool enqueue(const session::Command& command,std::uint64_t& ticket)noexcept override{
    if(!accept)return false;
    last=command;ticket=++enqueued;return true;
  }
  bool set_running(bool value)noexcept override{running=value;return !stopping;}
  void cancel()noexcept override{cancelled=true;}
  void request_stop()noexcept override{stopping=true;}
  std::string snapshot(std::uint64_t revision=0,std::uint32_t delay=0)override{
    after=revision;wait=delay;return state;
  }
};

int main(){
  // Cached reads remain available while the owner is blocked in execute().
  {
    FakeDriver driver;driver.block.store(true);
    session::SessionWakeup wake;
    session::SessionHostWorker worker(driver,wake,10000);
    session::SessionCommandService commands(worker);
    CHECK(text(commands.handle("STATE")).find("\"completed\":0")!=std::string::npos);
    CHECK(text(commands.handle("ANALYZE 101 3 4 8")).starts_with("QUEUED "));
    CHECK(eventually([&]{return driver.entered.load();}));
    auto reader=std::async(std::launch::async,[&]{return text(commands.handle("STATE"));});
    const auto status=reader.wait_for(500ms);
    if(status!=std::future_status::ready)driver.release();
    CHECK(status==std::future_status::ready);
    CHECK(reader.get().find("\"completed\":0")!=std::string::npos);

    // Exactly 32 additional commands can wait behind the active operation.
    for(unsigned i=0;i<32;++i){std::uint64_t ticket=0;CHECK(worker.enqueue({},ticket)&&ticket>0);}
    std::uint64_t rejected_ticket=0;
    CHECK(!worker.enqueue({},rejected_ticket));
    CHECK(text(commands.handle("STEP")).starts_with("ERR command queue full"));
    CHECK(text(commands.handle("CANCEL"))=="OK CANCEL\n");
    driver.release();
    CHECK(eventually([&]{return driver.saw_cancel.load();}));
    worker.shutdown();
    CHECK(driver.calls.load()==1&&driver.completed.load()==0);
  }
  // A control-only change must wake WATCH even with a distant cadence deadline.
  {
    FakeDriver driver;session::SessionWakeup wake;
    session::SessionHostWorker worker(driver,wake,10000);
    const auto old=view_revision(worker.snapshot());
    auto watching=std::async(std::launch::async,[&]{return worker.snapshot(old,1500);});
    CHECK(worker.set_running(true));
    CHECK(watching.wait_for(500ms)==std::future_status::ready);
    const auto changed=watching.get();
    CHECK(view_revision(changed)>old&&changed.find("\"running\":true")!=std::string::npos);
    CHECK(driver.completed.load()==0);
    CHECK(worker.set_running(false));worker.shutdown();
  }
  // Automatic cadence runs, then remains paused after its in-flight boundary.
  {
    FakeDriver driver;session::SessionWakeup wake;
    session::SessionHostWorker worker(driver,wake,15);
    worker.snapshot();CHECK(worker.set_running(true));
    CHECK(eventually([&]{return driver.completed.load()>=3;}));
    CHECK(worker.set_running(false));
    std::this_thread::sleep_for(50ms);
    const auto paused=driver.completed.load();
    std::this_thread::sleep_for(60ms);
    CHECK(driver.completed.load()==paused);
    worker.shutdown();
  }
  // Cancellation belongs to the old operation; a later RUN can start new work.
  {
    FakeDriver driver;driver.block.store(true);session::SessionWakeup wake;
    session::SessionHostWorker worker(driver,wake,15);
    worker.snapshot();session::Command analyze;analyze.operation=session::Operation::analyze;
    std::uint64_t ticket=0;CHECK(worker.enqueue(analyze,ticket));
    CHECK(eventually([&]{return driver.entered.load();}));
    worker.cancel();driver.release();
    CHECK(eventually([&]{return driver.saw_cancel.load();}));
    driver.block.store(false);CHECK(worker.set_running(true));
    CHECK(eventually([&]{return driver.completed.load()>0;}));
    CHECK(worker.set_running(false));worker.shutdown();
  }
  // Shutdown completes the current cycle while retaining recorded future work.
  {
    FakeDriver driver;driver.record_targets.store(true);session::SessionWakeup wake;
    session::SessionHostWorker worker(driver,wake,10000);
    worker.snapshot();session::Command step;step.count=5;
    std::uint64_t ticket=0;CHECK(worker.enqueue(step,ticket));
    CHECK(eventually([&]{return driver.active.load()&&driver.target.load()==5;}));
    worker.request_stop();driver.allow_pump.store(true);wake.notify();
    worker.shutdown();
    CHECK(driver.completed.load()==1&&driver.target.load()==5&&!driver.active.load());
    CHECK(worker.snapshot().find("\"stopping\":true")!=std::string::npos);
    CHECK(!worker.enqueue({},ticket)&&!worker.set_running(true));
  }
  // Parser never silently accepts trailing commands or malformed numbers.
  {
    RecordingControl control;session::SessionCommandService commands(control);
    CHECK(text(commands.handle("PING"))=="PONG\n");
    CHECK(text(commands.handle("STEP")).starts_with("QUEUED ")&&control.last.count==1);
    CHECK(text(commands.handle("STEP 1024")).starts_with("QUEUED ")&&control.last.count==1024);
    CHECK(text(commands.handle("ANALYZE 101 3 4 128")).starts_with("QUEUED "));
    CHECK(control.last.operation==session::Operation::analyze&&control.last.engine==101&&
      control.last.from==3&&control.last.to==4&&control.last.count==128);
    CHECK(text(commands.handle("APPLY")).starts_with("QUEUED ")&&control.last.operation==session::Operation::apply);
    CHECK(text(commands.handle("REVERT 101")).starts_with("QUEUED ")&&control.last.operation==session::Operation::revert);
    CHECK(text(commands.handle("RETRY")).starts_with("QUEUED ")&&control.last.operation==session::Operation::retry);
    const auto accepted=control.enqueued;
    for(const char* invalid:{"", "PING junk", "RUN junk", "PAUSE junk", "CANCEL junk", "STOP junk",
      "STEP 0", "STEP 1025", "STEP -1", "STEP +1", "STEP 1.0", "STEP 1 2", "STEP 4294967296",
      "ANALYZE 1 3 4 0", "ANALYZE 1 3 4 129", "ANALYZE 1 3 4 8 junk", "ANALYZE 1 3 4",
      "APPLY junk", "REVERT", "REVERT x", "REVERT 1 junk", "RETRY junk", "WATCH -1",
      "WATCH 18446744073709551616", "WATCH 1 junk"})
      CHECK(text(commands.handle(invalid)).starts_with("ERR "));
    CHECK(control.enqueued==accepted&&!control.stopping&&!control.cancelled&&!control.running);
    CHECK(text(commands.handle("WATCH 42"))=="{}\n"&&control.after==42&&control.wait==500);
    control.state=std::string(host::kMaxReplyBytes-1,'x')+"\n";
    CHECK(commands.handle("STATE").length==host::kMaxReplyBytes);
    control.state=std::string(host::kMaxReplyBytes,'x')+"\n";
    CHECK(text(commands.handle("STATE"))=="ERR response exceeds bounded frame\n");
    control.accept=false;
    CHECK(text(commands.handle("STEP")).starts_with("ERR command queue full"));
    CHECK(text(commands.handle("CANCEL"))=="OK CANCEL\n"&&control.cancelled);
    const auto stopped=commands.handle("STOP");
    CHECK(stopped.stop&&text(stopped)=="BYE\n"&&control.stopping);
  }
  std::cout<<"session worker cached reads, capacity, cadence, watches, cancellation, shutdown and commands PASS\n";
}
