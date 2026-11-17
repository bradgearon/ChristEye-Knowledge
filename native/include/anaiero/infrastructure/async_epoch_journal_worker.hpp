#pragma once
// One bounded asynchronous storage worker. Producer never blocks on fsync.
// Crash between journal commit and projection recovers by WindStore replay.
#include "anaiero/contracts/async_observation_store.hpp"
#include <array>
#include <chrono>
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
  IAsyncCompletionSignal* completion_signal_=nullptr;
  mutable std::mutex mutex_{};
  std::condition_variable cv_{};
  std::array<Work,Capacity> queue_{};
  std::array<AsyncCommitReceipt,Capacity> receipts_{};
  std::size_t input_head_=0,input_tail_=0,input_count_=0;
  std::size_t result_head_=0,result_tail_=0,result_count_=0;
  std::size_t active_=0,outstanding_=0;
  std::uint64_t next_ticket_=1,accepted_=0,durable_=0,failed_=0,rejected_=0;
  bool stopping_=false,stopped_=false,write_failed_=false;
  std::thread worker_;
  void notify_completion() noexcept {
    cv_.notify_all();
    if(completion_signal_)completion_signal_->notify();
  }
  void loop() noexcept {
    while(true){
      Work next{};
      bool blocked=false;
      {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock,[&]{return stopping_||input_count_!=0;});
        if(input_count_==0&&stopping_)break;
        next=queue_[input_head_];
        input_head_=(input_head_+1)%Capacity;
        --input_count_;
        active_=1;
        blocked=write_failed_;
      }
      std::array<std::byte,kEpochObservationFrame> frame{};
      AsyncCommitOutcome outcome=AsyncCommitOutcome::blocked_by_previous_failure;
      if(!blocked){
        if(!codec_.encode(next.event,frame))outcome=AsyncCommitOutcome::encoding_failed;
        else if(!journal_.commit(next.event,frame))outcome=AsyncCommitOutcome::persistence_failed;
        else outcome=AsyncCommitOutcome::durable;
      }
      const bool committed=outcome==AsyncCommitOutcome::durable;
      {
        std::lock_guard<std::mutex> lock(mutex_);
        // Admission reserves a receipt slot for EVERY accepted operation.
        // Shutdown never requires a consumer to make room or discards a
        // completion, including an unsuccessful completion.
        receipts_[result_tail_]={next.ticket,next.event,committed,outcome};
        result_tail_=(result_tail_+1)%Capacity;
        ++result_count_;active_=0;
        if(committed)++durable_;
        else {++failed_;write_failed_=true;}
      }
      notify_completion();
    }
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopped_=true;
    }
    notify_completion();
  }
public:
  AsyncEpochJournalWorker(IObservationEpochJournal& journal,
                          IObservationEpochCodec& codec,
                          IAsyncCompletionSignal* signal=nullptr):
    journal_(journal),codec_(codec),completion_signal_(signal),
    worker_([this]{loop();}){}
  AsyncEpochJournalWorker(const AsyncEpochJournalWorker&)=delete;
  AsyncEpochJournalWorker&operator=(const AsyncEpochJournalWorker&)=delete;
  ~AsyncEpochJournalWorker(){shutdown();}
  bool enqueue(const Observation& event,std::uint64_t& ticket) noexcept override {
    ticket=0;
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_||write_failed_||outstanding_==Capacity||next_ticket_==0){
      ++rejected_;return false;
    }
    ticket=next_ticket_++;
    queue_[input_tail_]={event,ticket};
    input_tail_=(input_tail_+1)%Capacity;
    ++input_count_;
    ++outstanding_;++accepted_;
    // Reader and worker waits share cv_: notify_one could wake the wrong one.
    cv_.notify_all();
    return true;
  }
  bool try_receive(AsyncCommitReceipt& result) noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);
    if(!result_count_)return false;
    result=receipts_[result_head_];
    result_head_=(result_head_+1)%Capacity;
    --result_count_;--outstanding_;
    return true;
  }
  std::size_t queued()const noexcept override {
    std::lock_guard<std::mutex> lock(mutex_);return input_count_;
  }
  // Stops admission and drains accepted work. It cannot cancel an OS disk
  // call or promise a deadline for stuck storage. Owner-thread only; do not
  // call shutdown from the completion signal.
  void request_stop()noexcept {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      stopping_=true;
    }
    cv_.notify_all();
  }
  void shutdown()noexcept {
    request_stop();
    if(worker_.joinable())worker_.join();
  }
  // Event-driven convenience for one consumer. False means timeout or a
  // fully stopped worker with no receipt remaining; there is no sleep loop.
  bool wait_for_completion(std::chrono::milliseconds timeout)noexcept {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait_for(lock,timeout,[&]{return result_count_!=0||stopped_;});
    return result_count_!=0;
  }
  AsyncObservationStoreStatus status()const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return {input_count_,active_,result_count_,outstanding_,Capacity,
      accepted_,durable_,failed_,rejected_,stopping_,stopped_,write_failed_};
  }
  // Compatibility accessor: completion reservations now make drops impossible.
  std::uint64_t dropped_completions()const noexcept {
    return 0;
  }
};
} // namespace anaiero::runtime
