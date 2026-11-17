#pragma once
// Single-owner asynchronous evidence coordinator. No callback is allowed
// to mark an observation projected before an actual durable receipt.
#include "anaiero/contracts/async_observation_store.hpp"
#include <cstdint>
namespace anaiero::runtime {
class AsyncObservationCommitService final {
  IAsyncObservationStore& worker_;
  IObservationEpochProjection& projector_;
  AsyncCommitReceipt pending_{};
  std::uint64_t ticket_=0;
  bool awaiting_=false,ready_to_project_=false;
public:
  AsyncObservationCommitService(IAsyncObservationStore& worker,
     IObservationEpochProjection& projector) noexcept:
     worker_(worker),projector_(projector){}
  bool submit(const Observation& value) noexcept {
    if(awaiting_||ready_to_project_||!projector_.can_accept(value))return false;
    if(!worker_.enqueue(value,ticket_))return false;
    awaiting_=true;return true;
  }
  bool poll() noexcept {
    if(awaiting_) {
      AsyncCommitReceipt result{};
      if(!worker_.try_receive(result))return false;
      if(result.ticket!=ticket_||!result.durable)return false;
      pending_=result;ready_to_project_=true;awaiting_=false;
    }
    if(ready_to_project_) {
      if(!projector_.apply(pending_.event))return false;
      ready_to_project_=false;
      pending_={};ticket_=0;
      return true;
    }
    return false;
  }
  bool pending()const noexcept{return awaiting_||ready_to_project_;}
};
} // namespace anaiero::runtime
