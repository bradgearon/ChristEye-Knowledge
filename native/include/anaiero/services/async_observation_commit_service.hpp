#pragma once
// Single-owner asynchronous evidence coordinator. Completion wakeups schedule
// poll() on the owner; they never project on the disk-worker thread.
#include "anaiero/contracts/async_observation_store.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
namespace anaiero::runtime {
enum class AsyncObservationFailure : std::uint8_t {
  none, projection_retry, persistence, receipt_protocol
};
struct AsyncObservationCommitStatus {
  std::size_t outstanding=0,awaiting=0,durable_ready=0,failed_entries=0,capacity=0;
  std::uint64_t submitted=0,received=0,durable=0,projected=0,rejected=0;
  std::uint64_t projection_retries=0,persistence_failures=0,protocol_failures=0;
  bool closed=false,failed=false,configuration_valid=true;
  AsyncObservationFailure failure=AsyncObservationFailure::none;
};
class AsyncObservationCommitService final {
public:
  static constexpr std::size_t maximum_inflight=256;
private:
  struct Pending {
    AsyncCommitReceipt receipt{};
    bool received=false;
  };
  IAsyncObservationStore& worker_;
  IObservationEpochProjection& projector_;
  contracts::IServiceExtension* extension_=nullptr;
  std::array<Pending,maximum_inflight> pending_{};
  std::size_t head_=0,count_=0,capacity_=1;
  AsyncObservationCommitStatus counters_{};
  AsyncCommitReceipt last_failure_{};
  AsyncObservationFailure failure_=AsyncObservationFailure::none;
  bool closed_=false,failed_=false,have_failure_receipt_=false;
  static bool same(const Observation& a,const Observation& b) noexcept {
    return a.source==b.source&&a.source_epoch==b.source_epoch&&
      a.sequence==b.sequence&&a.colony==b.colony&&a.animon==b.animon&&
      a.metric==b.metric&&a.lane==b.lane&&a.origin==b.origin&&
      a.clock_domain==b.clock_domain&&a.value==b.value&&
      a.urgency==b.urgency&&a.importance==b.importance&&
      a.evidence_tag==b.evidence_tag&&a.acquired_ns==b.acquired_ns;
  }
  void notify(contracts::ServicePhase phase,const AsyncCommitReceipt& receipt,
              std::int32_t status) noexcept {
    if(extension_)extension_->on_service_event({contracts::ServiceId::evidence,
      phase,receipt.ticket,receipt.event.animon,status});
  }
  void fail(AsyncObservationFailure kind,const AsyncCommitReceipt& receipt)noexcept {
    // Keep the original terminal failure rather than a later blocked receipt.
    if(!failed_||(kind==AsyncObservationFailure::receipt_protocol&&
                  failure_!=AsyncObservationFailure::receipt_protocol)){
      last_failure_=receipt;failure_=kind;have_failure_receipt_=true;
    }
    failed_=true;
  }
public:
  AsyncObservationCommitService(IAsyncObservationStore& worker,
     IObservationEpochProjection& projector,std::size_t max_inflight=1,
     contracts::IServiceExtension* extension=nullptr) noexcept:
     worker_(worker),projector_(projector),extension_(extension),
     capacity_(max_inflight<=maximum_inflight?max_inflight:0){}
  // Set before owner-loop startup. Trusted bounded observers must not reenter.
  void set_extension(contracts::IServiceExtension* extension)noexcept {
    extension_=extension;
  }
  bool submit(const Observation& value) noexcept {
    if(closed_||failed_||count_==capacity_||!projector_.can_accept(value)){
      ++counters_.rejected;return false;
    }
    // The live projector cannot see pending source-order reservations yet.
    for(std::size_t offset=count_;offset>0;--offset){
      const auto& prior=pending_[(head_+offset-1)%maximum_inflight].receipt.event;
      if(prior.source!=value.source||prior.source_epoch!=value.source_epoch)continue;
      if(prior.sequence==UINT64_MAX||value.sequence!=prior.sequence+1||
         value.acquired_ns<prior.acquired_ns||value.clock_domain!=prior.clock_domain){
        ++counters_.rejected;return false;
      }
      break;
    }
    std::uint64_t ticket=0;
    if(!worker_.enqueue(value,ticket)){++counters_.rejected;return false;}
    const auto index=(head_+count_)%maximum_inflight;
    pending_[index]={{ticket,value,false,AsyncCommitOutcome::unspecified},false};
    ++count_;++counters_.submitted;
    notify(contracts::ServicePhase::proposed,pending_[index].receipt,0);
    // Invalid/reused tickets violate the store contract. Preserve the already
    // accepted operation for diagnosis rather than reporting it unaccepted.
    for(std::size_t i=0;i+1<count_;++i)
      if(ticket==pending_[(head_+i)%maximum_inflight].receipt.ticket){
        ++counters_.protocol_failures;
        fail(AsyncObservationFailure::receipt_protocol,pending_[index].receipt);
        return true;
      }
    if(ticket==0){
      ++counters_.protocol_failures;
      fail(AsyncObservationFailure::receipt_protocol,pending_[index].receipt);
    }
    return true;
  }
  // Drain-on-wakeup; true means at least one observation became projected.
  // Out-of-order store completions are buffered; projection follows submit
  // order. A failed apply stays at the head for a later owner retry, with no
  // duplicate durable write and no overtaking by later observations.
  bool poll() noexcept {
    for(std::size_t drained=0;drained<maximum_inflight;++drained){
      AsyncCommitReceipt receipt{};
      if(!worker_.try_receive(receipt))break;
      ++counters_.received;
      Pending* entry=nullptr;
      for(std::size_t i=0;i<count_;++i){
        auto& candidate=pending_[(head_+i)%maximum_inflight];
        if(candidate.receipt.ticket==receipt.ticket){entry=&candidate;break;}
      }
      if(!entry||entry->received||!same(entry->receipt.event,receipt.event)||
         (receipt.durable&&receipt.outcome!=AsyncCommitOutcome::unspecified&&
          receipt.outcome!=AsyncCommitOutcome::durable)||
         (!receipt.durable&&receipt.outcome==AsyncCommitOutcome::durable)){
        ++counters_.protocol_failures;
        fail(AsyncObservationFailure::receipt_protocol,receipt);
        notify(contracts::ServicePhase::rejected,receipt,
          static_cast<std::int32_t>(AsyncObservationFailure::receipt_protocol));
        continue;
      }
      entry->receipt=receipt;entry->received=true;
      if(receipt.durable){
        ++counters_.durable;
        notify(contracts::ServicePhase::reviewed,receipt,0);
      }else{
        ++counters_.persistence_failures;
        fail(AsyncObservationFailure::persistence,receipt);
        notify(contracts::ServicePhase::rejected,receipt,
          static_cast<std::int32_t>(AsyncObservationFailure::persistence));
      }
    }
    // An invalid receipt makes the entire relationship untrustworthy. Valid
    // durable predecessors of an ordinary persistence failure may project.
    if(failed_&&failure_==AsyncObservationFailure::receipt_protocol)return false;
    bool progressed=false;
    while(count_){
      auto& next=pending_[head_];
      if(!next.received||!next.receipt.durable)break;
      if(!projector_.apply(next.receipt.event)){
        ++counters_.projection_retries;
        if(!failed_){
          failure_=AsyncObservationFailure::projection_retry;
          last_failure_=next.receipt;have_failure_receipt_=true;
        }
        notify(contracts::ServicePhase::rejected,next.receipt,
          static_cast<std::int32_t>(AsyncObservationFailure::projection_retry));
        break;
      }
      ++counters_.projected;progressed=true;
      const auto projected=next.receipt;
      next={};head_=(head_+1)%maximum_inflight;--count_;
      if(!failed_){failure_=AsyncObservationFailure::none;have_failure_receipt_=false;}
      notify(contracts::ServicePhase::committed,projected,0);
    }
    return progressed;
  }
  // Close rejects new submissions. Accepted work remains drainable, including
  // failures; no accepted event is silently cancelled or cleared.
  void close()noexcept {closed_=true;}
  bool pending()const noexcept{return count_!=0;}
  bool failed()const noexcept{return failed_;}
  std::size_t outstanding()const noexcept{return count_;}
  std::size_t capacity()const noexcept{return capacity_;}
  AsyncObservationFailure failure_kind()const noexcept{return failure_;}
  bool failure_receipt(AsyncCommitReceipt& receipt)const noexcept {
    if(!have_failure_receipt_)return false;
    receipt=last_failure_;return true;
  }
  // Read retained accepted work for diagnosis/recovery, on the owner thread.
  // Inspection neither deletes it nor changes a failure into an ACK.
  bool inspect(std::size_t offset,AsyncCommitReceipt& receipt,
               bool& received)const noexcept {
    if(offset>=count_)return false;
    const auto& entry=pending_[(head_+offset)%maximum_inflight];
    receipt=entry.receipt;received=entry.received;return true;
  }
  AsyncObservationCommitStatus status()const noexcept {
    auto result=counters_;
    result.outstanding=count_;result.capacity=capacity_;
    result.closed=closed_;result.failed=failed_;
    result.configuration_valid=capacity_!=0;result.failure=failure_;
    for(std::size_t i=0;i<count_;++i){
      const auto& entry=pending_[(head_+i)%maximum_inflight];
      if(!entry.received)++result.awaiting;
      else if(entry.receipt.durable)++result.durable_ready;
      else ++result.failed_entries;
    }
    return result;
  }
};
} // namespace anaiero::runtime
