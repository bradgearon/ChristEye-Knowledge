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
  ObservationJournalCapacity capacity_{};
  std::thread worker_;
  bool capacity_locked(std::uint64_t events,std::uint64_t bytes)const noexcept {
    if(stopping_||write_failed_||!capacity_.writable)return false;
    if(!capacity_.known)return true;
    if(!capacity_.append_reservation_bytes)return false;
    const auto reserved=static_cast<std::uint64_t>(input_count_+active_);
    if(events>capacity_.available_events||reserved>capacity_.available_events-events||
       reserved>UINT64_MAX/capacity_.append_reservation_bytes)return false;
    const auto held_bytes=reserved*capacity_.append_reservation_bytes;
    return bytes<=capacity_.available_bytes&&held_bytes<=capacity_.available_bytes-bytes;
  }
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
      // Registry access remains on its worker owner. Publish only a plain-data
      // capacity snapshot under the queue mutex, never hold it across file IO.
      const auto capacity=journal_.storage_capacity();
      {
        std::lock_guard<std::mutex> lock(mutex_);
        capacity_=capacity;
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
    journal_(journal),codec_(codec),completion_signal_(signal),capacity_(journal.storage_capacity()),
    worker_([this]{loop();}){}
  AsyncEpochJournalWorker(const AsyncEpochJournalWorker&)=delete;
  AsyncEpochJournalWorker&operator=(const AsyncEpochJournalWorker&)=delete;
  ~AsyncEpochJournalWorker(){shutdown();}
  bool enqueue(const Observation& event,std::uint64_t& ticket) noexcept override {
    ticket=0;
    std::lock_guard<std::mutex> lock(mutex_);
    if(stopping_||write_failed_||outstanding_==Capacity||next_ticket_==0||
       !capacity_locked(1,capacity_.append_reservation_bytes)){
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
  ObservationJournalCapacity storage_capacity()const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);auto result=capacity_;
    const auto held=static_cast<std::uint64_t>(input_count_+active_);
    result.available_events=result.available_events>=held?result.available_events-held:0;
    const auto bytes=result.append_reservation_bytes&&held<=UINT64_MAX/result.append_reservation_bytes?held*result.append_reservation_bytes:UINT64_MAX;
    result.available_bytes=result.available_bytes>=bytes?result.available_bytes-bytes:0;
    result.writable=result.writable&&!stopping_&&!write_failed_;return result;
  }
  bool has_capacity(std::size_t events,std::size_t bytes)const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);return capacity_locked(events,bytes);
  }
  // Compatibility accessor: completion reservations now make drops impossible.
  std::uint64_t dropped_completions()const noexcept {
    return 0;
  }
};
} // namespace anaiero::runtime
