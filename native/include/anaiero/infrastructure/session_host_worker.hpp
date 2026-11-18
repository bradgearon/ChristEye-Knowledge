#pragma once
// Threading adapter: one serial domain owner, immutable cached IPC views.
#include "anaiero/contracts/session_control.hpp"
#include "anaiero/infrastructure/session_wakeup.hpp"
#include "anaiero/infrastructure/session_view_encoder.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <sstream>
#include <thread>
namespace anaiero::session {
class SessionHostWorker final:public IControl,private ICancellation {
  struct Work {Command command{};std::uint64_t ticket=0;};
  IDriver& driver_;SessionWakeup& signal_;
  std::chrono::milliseconds period_;
  mutable std::mutex mutex_;
  std::condition_variable view_changed_;
  std::array<Work,32> queue_{};
  std::size_t head_=0,count_=0;
  std::uint64_t next_ticket_=1,last_ticket_=0,view_revision_=0;
  std::string cached_,last_result_="idle";
  std::string trace_,experiment_;
  operations::View operations_{};
  bool running_=false,stopping_=false,stopped_=false;
  std::atomic<bool> cancellation_{false};
  std::thread owner_;
  bool cancelled()const noexcept override{return cancellation_.load(std::memory_order_acquire);}
  void update(){
    auto state=driver_.state_json();
    auto operations=driver_.operation_view();
    auto trace=driver_.trace_json();
    auto experiment=encode_experiment(driver_.experiment_view());
    {std::lock_guard<std::mutex> lock(mutex_);cached_=std::move(state);operations_=std::move(operations);
      trace_=std::move(trace);experiment_=std::move(experiment);++view_revision_;}
    view_changed_.notify_all();
  }
  void loop()noexcept {
    auto next=std::chrono::steady_clock::now()+period_;
    bool cancellation_observed=false;
    update();
    while(true){
      const auto observed=signal_.generation();
      bool stopping=false,running=false;Work work{};bool have_work=false,automatic=false;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping=stopping_;running=running_;
        if(!stopping&&count_&&(!driver_.busy()||driver_.failed())){
          work=queue_[head_];head_=(head_+1)%queue_.size();--count_;have_work=true;
          cancellation_.store(false,std::memory_order_release);
          cancellation_observed=false;
        }
        if(!have_work&&!stopping&&running&&!driver_.busy()&&!driver_.failed()&&
           next_ticket_!=UINT64_MAX&&std::chrono::steady_clock::now()>=next){
          automatic=true;cancellation_.store(false,std::memory_order_release);
          cancellation_observed=false;
          work.command.operation_id=next_ticket_++;
        }
      }
      if(!stopping&&!cancellation_observed&&cancellation_.load(std::memory_order_acquire)){
        driver_.cancel_operations();cancellation_observed=true;update();
      }
      if(stopping&&(driver_.quiescent()||driver_.failed()))break;
      bool progress=false;
      if(have_work){
        const bool accepted=driver_.execute(work.command,this);
        {std::lock_guard<std::mutex> lock(mutex_);last_ticket_=work.ticket;
          last_result_=accepted?"acknowledged":"rejected";}
        progress=true;
      }else if(!driver_.failed()&&driver_.busy())progress=driver_.pump();
      else if(automatic){
        progress=driver_.execute(work.command,this);
        next=std::chrono::steady_clock::now()+period_;
      }
      if(progress||driver_.failed())update();
      if(!progress){
        const auto deadline=(!stopping&&running&&!driver_.busy()&&!driver_.failed())?
          next:std::chrono::steady_clock::time_point::max();
        signal_.wait(observed,deadline);
      }
    }
    update();
    {std::lock_guard<std::mutex> lock(mutex_);stopped_=true;running_=false;}
    view_changed_.notify_all();
  }
public:
  SessionHostWorker(IDriver& driver,SessionWakeup& signal,std::uint32_t period_ms):
    driver_(driver),signal_(signal),period_(period_ms),next_ticket_(driver.operation_view().next_id),owner_([this]{loop();}){}
  ~SessionHostWorker(){shutdown();}
  bool enqueue(const Command& command,std::uint64_t& ticket)noexcept override{
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_||count_==queue_.size()||next_ticket_==0)return false;
    auto next=command;
    if(!next.operation_id)next.operation_id=std::max(next_ticket_,operations_.next_id);
    if(next.operation_id==UINT64_MAX||next.operation_id<=operations_.expired_through)return false;
    ticket=next.operation_id;next_ticket_=std::max(next_ticket_,ticket+1);
    queue_[(head_+count_)%queue_.size()]={std::move(next),ticket};++count_;
    ++view_revision_;view_changed_.notify_all();
    signal_.notify();return true;
  }
  bool set_running(bool value)noexcept override{
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_)return false;
    running_=value;++view_revision_;view_changed_.notify_all();signal_.notify();return true;
  }
  void cancel()noexcept override{
    {std::lock_guard<std::mutex> lock(mutex_);cancellation_.store(true,std::memory_order_release);
      count_=0;running_=false;last_result_="cancel_requested";++view_revision_;}
    driver_.request_cancel();
    view_changed_.notify_all();
    signal_.notify();
  }
  void request_stop()noexcept override{
    {std::lock_guard<std::mutex> lock(mutex_);cancellation_.store(true,std::memory_order_release);
      stopping_=true;running_=false;count_=0;++view_revision_;}
    view_changed_.notify_all();
    signal_.notify();
  }
  void shutdown()noexcept{request_stop();if(owner_.joinable())owner_.join();}
  std::string operation_result(std::uint64_t id)override {
    std::lock_guard<std::mutex> lock(mutex_);
    for(std::size_t i=0;i<operations_.count;++i)if(operations_.entries[i].id==id)return encode_operation(operations_.entries[i]);
    for(std::size_t i=0;i<count_;++i)if(queue_[(head_+i)%queue_.size()].ticket==id)
      return "{\"id\":"+std::to_string(id)+",\"state\":\"queued\",\"durable\":false}\n";
    return "{\"id\":"+std::to_string(id)+",\"error\":\""+(id<=operations_.expired_through?"expired":"unknown")+"\"}\n";
  }
  std::string trace()override{std::lock_guard<std::mutex> lock(mutex_);return trace_;}
  std::string experiment_result()override{std::lock_guard<std::mutex> lock(mutex_);return experiment_;}
  std::string snapshot(std::uint64_t after=0,std::uint32_t wait_ms=0)override{
    std::unique_lock<std::mutex> lock(mutex_);
    if(cached_.empty()||(wait_ms&&after>=view_revision_))
      view_changed_.wait_for(lock,std::chrono::milliseconds(wait_ms?wait_ms:1000),[&]{
        return stopped_||(!cached_.empty()&&view_revision_>after);});
    std::ostringstream out;
    out<<"{\"schema\":\"anaiero.host.session/1\",\"view_revision\":"<<view_revision_
       <<",\"running\":"<<(running_?"true":"false")<<",\"stopping\":"<<(stopping_?"true":"false")
       <<",\"queued_commands\":"<<count_<<",\"last_ticket\":"<<last_ticket_
       <<",\"last_result\":\""<<last_result_<<"\",\"active_operation\":"<<operations_.active_id
       <<",\"next_operation_id\":"<<std::max(next_ticket_,operations_.next_id)
       <<",\"operation_results_retained\":"<<operations_.count<<",\"session\":"<<(cached_.empty()?"null":cached_)<<"}\n";
    return out.str();
  }
};
} // namespace anaiero::session
