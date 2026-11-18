#pragma once
#include "anaiero/contracts/async_observation_store.hpp"
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
namespace anaiero::session {
class SessionWakeup final:public runtime::IAsyncCompletionSignal {
  mutable std::mutex mutex_;
  std::condition_variable changed_;
  std::uint64_t generation_=0;
  runtime::IAsyncCompletionSignal* observer_=nullptr;
public:
  explicit SessionWakeup(runtime::IAsyncCompletionSignal* observer=nullptr):observer_(observer){}
  void notify()noexcept override{
    {std::lock_guard<std::mutex> lock(mutex_);++generation_;}
    changed_.notify_all();
    if(observer_)observer_->notify();
  }
  std::uint64_t generation()const noexcept{
    std::lock_guard<std::mutex> lock(mutex_);return generation_;
  }
  void wait(std::uint64_t previous,std::chrono::steady_clock::time_point deadline){
    std::unique_lock<std::mutex> lock(mutex_);
    changed_.wait_until(lock,deadline,[&]{return generation_!=previous;});
  }
};
} // namespace anaiero::session
