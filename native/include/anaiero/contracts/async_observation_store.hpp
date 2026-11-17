#pragma once
// Asynchronous disk delivery port. 'accepted' never means durable.
#include "anaiero/contracts/epoch_observation.hpp"
#include <cstddef>
#include <cstdint>
namespace anaiero::runtime {
struct AsyncCommitReceipt {
  std::uint64_t ticket=0;
  Observation event{};
  bool durable=false;
};
struct IAsyncObservationStore {
  virtual ~IAsyncObservationStore()=default;
  virtual bool enqueue(const Observation&,std::uint64_t& ticket) noexcept=0;
  virtual bool try_receive(AsyncCommitReceipt&) noexcept=0;
  virtual std::size_t queued()const noexcept=0;
};
} // namespace anaiero::runtime
