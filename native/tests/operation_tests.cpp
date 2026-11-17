#include "anaiero/services/operation_service.hpp"
#include "anaiero/infrastructure/operation_store.hpp"
#include <cstdlib>
#include <condition_variable>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <thread>
#define CHECK(x) do{if(!(x)){std::cerr<<"FAILED " #x " at "<<__LINE__<<'\n';std::abort();}}while(false)
namespace s=anaiero::session;
namespace op=anaiero::operations;
namespace db=christeye::living::durable;
namespace fs=std::filesystem;
struct Store final:op::IStore {
  op::Ledger persisted{};bool exists=false;unsigned attempts=0,fail_on=0;
  bool load(op::Ledger& value,bool& found)noexcept override{found=exists;if(found)value=persisted;return true;}
  bool save(op::Ledger& value)noexcept override{
    ++attempts;if(attempts==fail_on||!op::valid(value)||value.revision!=(exists?persisted.revision:0))return false;
    ++value.revision;persisted=value;exists=true;return true;
  }
};
struct EffectState {std::uint64_t id=0,revision=0,effects=0;unsigned pending=0;bool failed=false;};
class Driver final:public s::IDriver {
  EffectState& state_;
  std::mutex mutex_;std::condition_variable changed_;bool entered_=false,interrupted_=false;
  void blocked(){
    std::unique_lock<std::mutex> lock(mutex_);entered_=true;changed_.notify_all();
    changed_.wait(lock,[&]{return interrupted_;});state_.pending=0;state_.failed=true;
  }
public:
  unsigned calls=0,cancellations=0,work_per_command=0;
  bool reject_before_stamp=false,fail_on_cancel=false,block_execute=false,block_pump=false;
  explicit Driver(EffectState& state):state_(state){}
  bool execute(const s::Command& command,const s::ICancellation*)noexcept override{
    ++calls;if(reject_before_stamp)return false;
    if(command.operation==s::Operation::retry){
      if(!state_.failed&&!state_.pending)return false;
      state_.id=command.operation_id;++state_.revision;state_.failed=false;return true;
    }
    state_.id=command.operation_id;++state_.revision;++state_.effects;state_.pending=work_per_command;state_.failed=false;
    if(block_execute){blocked();return false;}return true;
  }
  bool pump()noexcept override{if(!state_.pending)return false;if(block_pump){blocked();return false;}--state_.pending;++state_.revision;return true;}
  bool busy()const noexcept override{return state_.pending!=0;}
  bool quiescent()const noexcept override{return !busy();}
  bool failed()const noexcept override{return state_.failed;}
  std::string state_json()const override{return "{}";}
  std::uint64_t revision()const noexcept override{return state_.revision;}
  std::uint64_t operation_id()const noexcept override{return state_.id;}
  s::View view()const override{s::View out;out.revision=state_.revision;out.projected.revision=state_.revision;out.error=state_.failed?"engine_cancelled":"";return out;}
  void cancel_operations()noexcept override{++cancellations;state_.pending=0;if(fail_on_cancel)state_.failed=true;}
  void request_cancel()noexcept override{std::lock_guard<std::mutex> lock(mutex_);interrupted_=true;changed_.notify_all();}
  bool await_blocked(){std::unique_lock<std::mutex> lock(mutex_);return changed_.wait_for(lock,std::chrono::seconds(3),[&]{return entered_;});}
};
static s::Command command(std::uint64_t id){s::Command out;out.operation=s::Operation::step;out.count=1;out.operation_id=id;return out;}
static void before_effect_windows(){
  {
    Store unsaved;unsaved.fail_on=2;EffectState state;Driver driver(state);s::OperationService host(driver,unsaved);
    CHECK(host.start()&&!host.execute(command(1),nullptr));
    const auto visible=host.operation_view();CHECK(visible.count==0&&visible.active_id==0&&visible.next_id==1&&driver.calls==0);
  }
  // Intent reached storage but the running transition failed before dispatch.
  Store store;store.fail_on=3;EffectState state;Driver first(state);
  {s::OperationService service(first,store);CHECK(service.start());CHECK(!service.execute(command(1),nullptr));CHECK(first.calls==0);CHECK(service.failed());}
  CHECK(store.persisted.items[0].receipt.state==op::State::accepted&&state.effects==0);
  store.fail_on=0;Driver restarted(state);s::OperationService service(restarted,store);CHECK(service.start());CHECK(service.pump());
  CHECK(restarted.calls==1&&state.effects==1&&service.operation_view().entries[0].state==op::State::completed);
  CHECK(service.execute(command(1),nullptr)&&restarted.calls==1&&state.effects==1);
  auto different=command(1);different.count=2;CHECK(!service.execute(different,nullptr));

  // Running receipt exists; dispatch returned before it could stamp an effect,
  // and the failure receipt was not committed. Recovery may safely dispatch.
  Store interrupted;interrupted.fail_on=4;EffectState empty;Driver no_effect(empty);no_effect.reject_before_stamp=true;
  {s::OperationService host(no_effect,interrupted);CHECK(host.start());CHECK(!host.execute(command(1),nullptr));CHECK(empty.effects==0);}
  CHECK(interrupted.persisted.items[0].receipt.state==op::State::running);
  interrupted.fail_on=0;Driver retry(empty);s::OperationService host(retry,interrupted);
  CHECK(host.start()&&host.pump()&&retry.calls==1&&empty.effects==1&&!host.busy());
}
static void after_effect_windows(){
  Store store;store.fail_on=4;EffectState state;Driver first(state);
  {s::OperationService service(first,store);CHECK(service.start());CHECK(!service.execute(command(42),nullptr));CHECK(first.calls==1&&state.effects==1);
    const auto visible=service.operation_view();
    CHECK(visible.active_id==42&&visible.count==1&&visible.entries[0].state==op::State::running&&visible.entries[0].result==op::Result::none);
    CHECK(visible.entries[0].session_revision==0); // no invented durable completion watermark
  }
  CHECK(store.persisted.active_id==42&&store.persisted.items[0].receipt.state==op::State::running);
  store.fail_on=0;Driver restarted(state);s::OperationService service(restarted,store);CHECK(service.start()&&service.pump());
  CHECK(restarted.calls==0&&state.effects==1&&!service.busy());
  CHECK(service.operation_view().entries[0].session_revision==state.revision);
  CHECK(service.execute(command(42),nullptr)&&restarted.calls==0);

  Store asynchronous;EffectState ongoing;Driver working(ongoing);working.work_per_command=2;
  {s::OperationService host(working,asynchronous);CHECK(host.start()&&host.execute(command(7),nullptr));CHECK(host.busy());}
  Driver continued(ongoing);s::OperationService host(continued,asynchronous);CHECK(host.start());
  CHECK(host.pump()&&host.busy());CHECK(host.pump()&&!host.busy());
  CHECK(continued.calls==0&&ongoing.effects==1&&host.operation_view().entries[0].state==op::State::completed);
}
static void cancellation_windows(){
  Store intent;intent.fail_on=3;EffectState empty;Driver first(empty);
  {s::OperationService host(first,intent);CHECK(host.start());CHECK(!host.execute(command(1),nullptr));}
  intent.fail_on=0;Driver waiting(empty);
  {s::OperationService host(waiting,intent);CHECK(host.start());host.cancel_operations();}
  CHECK(intent.persisted.items[0].receipt.result==op::Result::cancelled);
  Driver reopened(empty);s::OperationService host(reopened,intent);CHECK(host.start()&&host.pump());
  CHECK(reopened.calls==0&&empty.effects==0&&host.operation_view().entries[0].state==op::State::cancelled);

  Store running;EffectState work;Driver active(work);active.work_per_command=5;
  {s::OperationService service(active,running);CHECK(service.start()&&service.execute(command(2),nullptr));service.cancel_operations();}
  Driver restored(work);s::OperationService service(restored,running);CHECK(service.start()&&service.pump());
  CHECK(restored.calls==0&&work.effects==1&&!service.busy());
  CHECK(service.operation_view().entries[0].state==op::State::cancelled&&service.operation_view().entries[0].result==op::Result::cancelled);
  // Freshly restored session still has its old persisted target, while the
  // operation ledger already contains a terminal cancellation receipt.
  work.pending=5;work.failed=false;
  Driver old_target(work);s::OperationService recovered_target(old_target,running);
  CHECK(recovered_target.start()&&old_target.cancellations==1&&!recovered_target.busy());
  CHECK(old_target.calls==0&&work.effects==1&&recovered_target.operation_view().entries[0].state==op::State::cancelled);

  struct Cancel final:s::ICancellation {bool cancelled()const noexcept override{return true;}} cancel;
  Store cancelled;EffectState idle;Driver driver(idle);s::OperationService skipped(driver,cancelled);
  CHECK(skipped.start()&&skipped.execute(command(1),&cancel));CHECK(driver.calls==0&&idle.effects==0);
  CHECK(skipped.operation_view().entries[0].state==op::State::cancelled);
}
static void retention(){
  Store store;EffectState state;Driver driver(state);s::OperationService host(driver,store);CHECK(host.start());
  for(std::uint64_t id=10;id<=700;id+=10)CHECK(host.execute(command(id),nullptr));
  const auto before=host.operation_view();CHECK(before.count==64&&before.expired_through==60&&before.entries[0].id==70);
  CHECK(!host.execute(command(60),nullptr));CHECK(!host.execute(command(65),nullptr));
  const auto after=host.operation_view();
  CHECK(after.count==before.count&&after.expired_through==before.expired_through&&after.entries[0].id==70);
  CHECK(host.execute(command(70),nullptr)&&state.effects==70);
  Driver reopened(state);s::OperationService restored(reopened,store);CHECK(restored.start());
  CHECK(!restored.execute(command(60),nullptr)&&restored.execute(command(700),nullptr)&&reopened.calls==0);
  CHECK(restored.execute(command(0),nullptr));CHECK(state.id==701&&state.effects==71);
  CHECK(!restored.execute(command(UINT64_MAX),nullptr));
}
static void retry_failed_cycle(){
  Store store;EffectState state;Driver driver(state);driver.work_per_command=2;
  s::OperationService host(driver,store);CHECK(host.start()&&host.execute(command(1),nullptr));
  state.failed=true;auto retry=command(2);retry.operation=s::Operation::retry;
  CHECK(!host.execute(retry,nullptr)); // original active ledger entry must first settle
  CHECK(host.pump());CHECK(host.failed()&&host.busy());
  CHECK(host.operation_view().entries[0].state==op::State::failed&&host.operation_view().active_id==0);
  CHECK(!host.execute(command(2),nullptr));
  CHECK(host.execute(retry,nullptr)&&!host.failed()&&host.busy()&&state.pending==2);
  CHECK(host.pump()&&host.busy());CHECK(host.pump()&&!host.busy());
  const auto view=host.operation_view();
  CHECK(view.count==2&&view.entries[1].state==op::State::completed&&view.entries[1].result==op::Result::ok);
  CHECK(state.effects==1&&driver.calls==2&&state.id==2); // resume existing cycle, no duplicate effect
  auto new_retry=command(3);new_retry.operation=s::Operation::retry;
  CHECK(!host.execute(new_retry,nullptr)&&host.operation_view().count==2);
  CHECK(host.execute(retry,nullptr)&&driver.calls==2); // retained replay is still idempotent
}
static void retry_intent_restart(){
  for(unsigned failure:{6u,7u}){
    Store store;EffectState state;Driver original(state);original.work_per_command=2;
    auto retry=command(2);retry.operation=s::Operation::retry;
    {
      s::OperationService host(original,store);CHECK(host.start()&&host.execute(command(1),nullptr));
      state.failed=true;CHECK(host.pump()&&host.failed()&&state.pending==2);
      store.fail_on=failure;original.reject_before_stamp=true;
      CHECK(!host.execute(retry,nullptr));
      CHECK(store.persisted.active_id==2&&state.id==1&&state.effects==1);
      CHECK(store.persisted.items[1].receipt.state==(failure==6?op::State::accepted:op::State::running));
    }
    // Restart creates a fresh engine and clears its transient error, while its
    // durable cycle and previous operation stamp still await continuation.
    state.failed=false;store.fail_on=0;Driver fresh(state);s::OperationService recovered(fresh,store);
    CHECK(recovered.start()&&recovered.pump());
    CHECK(fresh.calls==1&&state.id==2&&state.pending==2&&state.effects==1);
    CHECK(recovered.pump()&&recovered.busy());CHECK(recovered.pump()&&!recovered.busy());
    CHECK(recovered.operation_view().entries[1].state==op::State::completed&&state.effects==1);
  }
}
static void store_cas(const fs::path& root){
  fs::remove_all(root);
  {
    db::Registry registry;CHECK(registry.open(root)==db::Code::ok);
    op::WindStoreOperationStore one(registry,"test-operations"),two(registry,"test-operations");
    op::Ledger a,b;bool exists=true;CHECK(one.load(a,exists)&&!exists&&two.load(b,exists)&&!exists);
    CHECK(one.save(a)&&a.revision==1);CHECK(!two.save(b)&&b.revision==0);
    CHECK(two.load(b,exists)&&exists&&b.revision==1);
    auto invalid=b;invalid.next_id=0;CHECK(!two.save(invalid));
    CHECK(one.save(a)&&a.revision==2);CHECK(!two.save(b)&&b.revision==1);
    CHECK(two.load(b,exists)&&b.revision==2);
    b.next_id=2;b.count=1;b.items[0].command=command(1);b.items[0].receipt.id=1;
    b.items[0].receipt.state=op::State::completed;b.items[0].receipt.result=op::Result::ok;
    CHECK(two.save(b)&&b.revision==3);
    const auto bytes=op::encode(b);op::Ledger decoded;CHECK(op::decode(bytes,decoded));
    CHECK(!op::decode(bytes+"extra\n",decoded));
    auto regressed=b;regressed.count=0;regressed.next_id=1;CHECK(!two.save(regressed));
  }
  {
    db::Registry registry;CHECK(registry.open(root)==db::Code::ok);
    op::WindStoreOperationStore store(registry,"test-operations");op::Ledger value;bool exists=false;
    CHECK(store.load(value,exists)&&exists&&value.revision==3&&value.items[0].receipt.id==1);
    EffectState state;state.id=1;state.effects=1;Driver driver(state);s::OperationService host(driver,store);
    CHECK(host.start()&&host.execute(command(1),nullptr)&&driver.calls==0&&state.effects==1);
  }
}
static void concurrent_cancellation(){
  for(bool execute:{false,true}){
    Store store;EffectState state;Driver driver(state);driver.work_per_command=2;
    driver.block_execute=execute;driver.block_pump=!execute;
    s::OperationService service(driver,store);CHECK(service.start());
    if(!execute)CHECK(service.execute(command(1),nullptr));
    bool accepted=false;
    std::thread owner([&]{accepted=execute?service.execute(command(1),nullptr):service.pump();});
    CHECK(driver.await_blocked());service.request_cancel();owner.join();
    CHECK(accepted&&!service.busy()&&service.failed()); // the killed engine epoch remains failed
    CHECK(service.operation_view().entries[0].state==op::State::cancelled&&
      service.operation_view().entries[0].result==op::Result::cancelled);
    CHECK(store.persisted.items[0].receipt.state==op::State::cancelled&&driver.calls==1);
  }
  // Cancellation classification is itself durable before the terminal receipt.
  Store store;EffectState state;Driver driver(state);driver.work_per_command=2;driver.block_pump=true;
  {s::OperationService service(driver,store);CHECK(service.start()&&service.execute(command(1),nullptr));
    store.fail_on=5; // start, intent, running, cancel marker, failed terminal write
    bool result=true;std::thread owner([&]{result=service.pump();});CHECK(driver.await_blocked());service.request_cancel();owner.join();CHECK(!result);
  }
  CHECK(store.persisted.active_id==1&&store.persisted.items[0].receipt.result==op::Result::cancelled);
  store.fail_on=0;Driver reopened(state);s::OperationService recovered(reopened,store);
  CHECK(recovered.start()&&recovered.pump());CHECK(reopened.calls==0&&state.effects==1);
  CHECK(recovered.operation_view().entries[0].state==op::State::cancelled);
}
int main(int argc,char**argv){
  CHECK(argc==2);before_effect_windows();after_effect_windows();cancellation_windows();concurrent_cancellation();retention();retry_failed_cycle();retry_intent_restart();store_cas(fs::absolute(argv[1]));
  std::cout<<"operation identity, effect recovery, cancellation, retained receipts and durable CAS PASS\n";
}
