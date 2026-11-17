#pragma once
// One bounded asynchronous storage worker. Producer never blocks on fsync.
// Crash between journal commit and projection recovers by WindStore replay.
#include "anaiero/contracts/async_observation_store.hpp"
#include <array>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <thread>
namespace anaiero::runtime {
template<std::size_t Capacity=32>
class AsyncEpochJournalWorker final:public IAsyncObservationStore {
  static_assert(Capacity>0&&Capacity<=256);
  struct Work {Observation event{};std::uint64_t ticket=0;};
  IObservationEpochJournal& journal_;
  IObservationEpochCodec& codec_;
  mutable std::mutex mutex_{};
  std::condition_variable cv_{};
  std::array<Work,Capacity> queue_{};
  std::array<AsyncCommitReceipt,Capacity> receipts_{};
  std::size_t input_head_=0,input_tail_=0,input_count_=0;
  std::size_t result_head_=0,result_tail_=0,result_count_=0;
  std::uint64_t next_ticket_=1,dropped_completions_=0;
  bool stopping_=false;
  std::thread worker_;
  void loop() noexcept {
    while(true){
      Work next{};
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock,[&]{return stopping_||input_count_!=0;});
        if(input_count_==0&&stopping_)break;
        next=queue_[input_head_];
        input_head_=(input_head_+1)%Capacity;
        --input_count_;
      }
      std::array<std::byte,kEpochObservationFrame> frame{};
      const bool committed=codec_.encode(next.event,frame)&&
                           journal_.commit(next.event,frame);
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock,[&]{return result_count_<Capacity||stopping_;});
        if(result_count_<Capacity){
          receipts_[result_tail_]={next.ticket,next.event,committed};
          result_tail_=(result_tail_+1)%Capacity;
          ++result_count_;
        }else {
          // Journaled events are still recoverable; report discarded
          // in-memory acknowledgements instead of claiming success.
          ++dropped_completions_;
        }
      }
      cv_.notify_all();
    }
  }
public:
  AsyncEpochJournalWorker(IObservationEpochJournal& journal,
                          IObservationEpochCodec& codec):
    journal_(journal),codec_(codec),worker_([this]{loop();}){}
  AsyncEpochJournalWorker(const AsyncEpochJournalWorker&)=delete;
  AsyncEpochJournalWorker&operator=(const AsyncEpochJournalWorker&)=delete;
  ~AsyncEpochJournalWorker(){shutdown();}
  bool enqueue(const Observation& event,std::uint64_t& ticket) noexcept override {
    ticket=0;
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_||input_count_==Capacity||next_ticket_==0)return false;
    ticket=next_ticket_++;
    queue_[input_tail_]={event,ticket};
    input_tail_=(input_tail_+1)%Capacity;
    ++input_count_;
    cv_.notify_one();
    return true;
  }
  bool try_receive(AsyncCommitReceipt& result) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!result_count_)return false;
    result=receipts_[result_head_];
    result_head_=(result_head_+1)%Capacity;
    --result_count_;
    cv_.notify_one();return true;
  }
  std::size_t queued()const noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);return input_count_;
  }
  void shutdown()noexcept {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      if(stopping_&&!worker_.joinable())return;
      stopping_=true;
    }
    cv_.notify_all();
    if(worker_.joinable())worker_.join();
  }
  std::uint64_t dropped_completions()const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);return dropped_completions_;
  }
};
} // namespace anaiero::runtime
