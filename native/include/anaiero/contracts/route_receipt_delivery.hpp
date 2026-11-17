#pragma once
// SW6.1: typed acknowledgement ports for *synthetic-only* digital route receipts.
// No physical observation authority, network identity, or actuator interface.
#include "anaiero/contracts/fork_optimization.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace anaiero::optimization {
inline constexpr std::size_t kRouteReceiptPayloadBytes=96;
inline constexpr std::size_t kRouteReceiptPacketBytes=56+kRouteReceiptPayloadBytes;
struct IRouteReceiptCodec {
  virtual ~IRouteReceiptCodec()=default;
  virtual bool encode(const RouteReceipt&,std::uint32_t source,
    std::array<std::byte,kRouteReceiptPacketBytes>&) noexcept=0;
  virtual bool decode(std::span<const std::byte>,std::uint32_t source,
    RouteReceipt&) noexcept=0;
};
struct IRouteReceiptReplay {
  virtual ~IRouteReceiptReplay()=default;
  // Borrowed frame: only valid during this call.
  virtual bool on_frame(std::span<const std::byte>) noexcept=0;
};
struct IRouteReceiptJournal {
  virtual ~IRouteReceiptJournal()=default;
  virtual bool append(const RouteReceipt&,std::span<const std::byte>) noexcept=0;
  virtual bool replay(IRouteReceiptReplay&) noexcept=0;
};
struct IRouteReceiptProjection {
  virtual ~IRouteReceiptProjection()=default;
  virtual bool initialize() noexcept=0;
  // Success means the requested event is visible in a coherent latest snapshot.
  virtual bool project(const RouteReceipt&) noexcept=0;
};
} // namespace anaiero::optimization
