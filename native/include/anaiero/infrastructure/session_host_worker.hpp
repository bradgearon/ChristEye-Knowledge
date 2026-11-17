#pragma once
// Threading adapter: one serial domain owner, immutable cached IPC views.
#include "anaiero/contracts/session_control.hpp"
#include "anaiero/infrastructure/session_wakeup.hpp"
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
  bool running_=false,stopping_=false,stopped_=false;
  std::atomic<bool> cancellation_{false};
  std::thread owner_;
  bool cancelled()const noexcept override{return cancellation_.load(std::memory_order_acquire);}
  void update(){
    auto state=driver_.state_json();
    {std::lock_guard<std::mutex> lock(mutex_);cached_=std::move(state);++view_revision_;}
    view_changed_.notify_all();
  }
  void loop()noexcept {
    auto next=std::chrono::steady_clock::now()+period_;
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
        }
        if(!have_work&&!stopping&&running&&!driver_.busy()&&!driver_.failed()&&
           std::chrono::steady_clock::now()>=next){
          automatic=true;cancellation_.store(false,std::memory_order_release);
        }
      }
      if(stopping&&(driver_.quiescent()||driver_.failed()))break;
      bool progress=false;
      if(have_work){
        const bool accepted=driver_.execute(work.command,this);
        {std::lock_guard<std::mutex> lock(mutex_);last_ticket_=work.ticket;
          last_result_=accepted?"executed":"rejected";}
        progress=true;
      }else if(!driver_.failed()&&driver_.busy())progress=driver_.pump();
      else if(automatic){
        progress=driver_.execute(Command{},this);
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
    driver_(driver),signal_(signal),period_(period_ms),owner_([this]{loop();}){}
  ~SessionHostWorker(){shutdown();}
  bool enqueue(const Command& command,std::uint64_t& ticket)noexcept override{
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_||count_==queue_.size()||next_ticket_==0)return false;
    ticket=next_ticket_++;queue_[(head_+count_)%queue_.size()]={command,ticket};++count_;
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
  std::string snapshot(std::uint64_t after=0,std::uint32_t wait_ms=0)override{
    std::unique_lock<std::mutex> lock(mutex_);
    if(cached_.empty()||(wait_ms&&after>=view_revision_))
      view_changed_.wait_for(lock,std::chrono::milliseconds(wait_ms?wait_ms:1000),[&]{
        return stopped_||(!cached_.empty()&&view_revision_>after);});
    std::ostringstream out;
    out<<"{\"schema\":\"anaiero.host.session/1\",\"view_revision\":"<<view_revision_
       <<",\"running\":"<<(running_?"true":"false")<<",\"stopping\":"<<(stopping_?"true":"false")
       <<",\"queued_commands\":"<<count_<<",\"last_ticket\":"<<last_ticket_
       <<",\"last_result\":\""<<last_result_<<"\",\"session\":"<<(cached_.empty()?"null":cached_)<<"}\n";
    return out.str();
  }
};
} // namespace anaiero::session
