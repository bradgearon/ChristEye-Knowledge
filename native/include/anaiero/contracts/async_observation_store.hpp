#pragma once
// Asynchronous disk delivery port. 'accepted' never means durable.
#include "anaiero/contracts/epoch_observation.hpp"
#include <cstddef>
#include <cstdint>
namespace anaiero::runtime {
// Acceptance is separate from durability. A failed writer reports later
// accepted work as blocked instead of appending after a source-sequence hole.
enum class AsyncCommitOutcome : std::uint8_t {
  unspecified, durable, encoding_failed, persistence_failed,
  blocked_by_previous_failure
};
struct AsyncCommitReceipt {
  std::uint64_t ticket=0;
  Observation event{};
  bool durable=false;
  AsyncCommitOutcome outcome=AsyncCommitOutcome::unspecified;
};
// Nonblocking owner-loop wakeup, never a callback into the projection service.
// Configure before starting the worker; signal must outlive shutdown().
// Receipts are authoritative. A host should retain a notification generation
// so a notification before its wait cannot be lost, then drain on its owner.
struct IAsyncCompletionSignal {
  virtual ~IAsyncCompletionSignal()=default;
  virtual void notify() noexcept=0;
};
struct AsyncObservationStoreStatus {
  std::size_t queued=0,active=0,receipts=0,outstanding=0,capacity=0;
  std::uint64_t accepted=0,durable=0,failed=0,rejected=0;
  bool stopping=false,stopped=false,write_failed=false;
};
struct IAsyncObservationStore {
  virtual ~IAsyncObservationStore()=default;
  virtual bool enqueue(const Observation&,std::uint64_t& ticket) noexcept=0;
  virtual bool try_receive(AsyncCommitReceipt&) noexcept=0;
  virtual std::size_t queued()const noexcept=0;
};
} // namespace anaiero::runtime
