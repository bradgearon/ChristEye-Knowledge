#pragma once
// CE-SW-01 SW1.1: bounded, allocation-free, single-owner scheduler.
// Fault *notifications* always preempt the weighted digital data lanes.
// Physical safety does not depend on this software scheduler.
#include <array>
#include <cstddef>
#include <cstdint>
#include "christeye/wind/packet.hpp"

namespace christeye::wind {
struct WorkItem {
  std::uint64_t token = 0;       // caller-owned immutable work descriptor
  std::uint64_t deadline_ns = 0; // 0 means no deadline, monotonic clock domain
};
enum class PostResult : std::uint8_t { accepted, full, invalid_class };
enum class TakeStatus : std::uint8_t { empty, ready, expired };
struct TakeResult {
  TakeStatus status = TakeStatus::empty;
  WorkItem item{};
  DeliveryClass delivery_class = DeliveryClass::status;
};

// This scheduler is single-owner. Cross-thread/ISR producers MUST serialize
// via a separate notification ring. Callbacks must be nonblocking; they may
// synchronously drain the scheduler on the same owner.
template<std::size_t PerClassCapacity>
class BoundedScheduler {
  static_assert(PerClassCapacity > 0, "nonzero per-class capacity required");
  static constexpr std::array<std::uint8_t, 5> weights_{{0, 6, 4, 3, 1}};
  struct Ring {
    std::array<WorkItem, PerClassCapacity> slots{};
    std::size_t read = 0;
    std::size_t write = 0;
    std::size_t count = 0;
    bool push(WorkItem v) noexcept {
      if (count == PerClassCapacity) return false;
      slots[write] = v;
      write = (write + 1) % PerClassCapacity;
      ++count;
      return true;
    }
    WorkItem pop() noexcept {
      const auto v = slots[read];
      read = (read + 1) % PerClassCapacity;
      --count;
      return v;
    }
  };
  std::array<Ring, 5> rings_{};
  std::array<std::int32_t, 5> credits_{};
  std::size_t count_ = 0;
  void (*notify_)(void*) = nullptr;
  void* notify_context_ = nullptr;

  TakeResult pop_class(std::size_t index, std::uint64_t now_ns) noexcept {
    const auto v = rings_[index].pop();
    --count_;
    if (rings_[index].count == 0) credits_[index] = 0;
    const bool expired = v.deadline_ns != 0 && now_ns >= v.deadline_ns;
    return {expired ? TakeStatus::expired : TakeStatus::ready,
            v, static_cast<DeliveryClass>(index)};
  }

 public:
  explicit BoundedScheduler(void (*notify)(void*) = nullptr, void* ctx = nullptr) noexcept
      : notify_(notify), notify_context_(ctx) {}
  BoundedScheduler(const BoundedScheduler&) = delete;
  BoundedScheduler& operator=(const BoundedScheduler&) = delete;

  PostResult post(DeliveryClass cls, WorkItem item) noexcept {
    const auto idx = static_cast<std::uint8_t>(cls);
    if (idx >= rings_.size()) return PostResult::invalid_class;
    const bool was_empty = count_ == 0;
    if (!rings_[idx].push(item)) return PostResult::full;
    ++count_;
    if (was_empty && notify_ != nullptr) notify_(notify_context_);
    return PostResult::accepted;
  }

  TakeResult next(std::uint64_t now_ns) noexcept {
    if (count_ == 0) return {};
    // Software fault reporting always preempts nonfault traffic; out-of-band
    // hardware interlocks remain the sole physical safety authority.
    if (rings_[0].count != 0) return pop_class(0, now_ns);

    // Smooth weighted round robin across active nonfault lanes. Interleave
    // service rather than emitting one large consecutive block per class.
    std::int32_t active_weight = 0;
    std::size_t selected = 0;
    bool found = false;
    for (std::size_t i = 1; i < rings_.size(); ++i) {
      if (rings_[i].count == 0) { credits_[i] = 0; continue; }
      const auto w = static_cast<std::int32_t>(weights_[i]);
      credits_[i] += w;
      active_weight += w;
      if (!found || credits_[i] > credits_[selected]) {
        selected = i;
        found = true;
      }
    }
    if (!found) return {};  // impossible unless internal counts corrupted
    credits_[selected] -= active_weight;
    return pop_class(selected, now_ns);
  }

  std::size_t pending() const noexcept { return count_; }
  std::size_t pending(DeliveryClass cls) const noexcept {
    const auto i = static_cast<std::uint8_t>(cls);
    return i < rings_.size() ? rings_[i].count : 0;
  }
  static constexpr std::size_t capacity() noexcept { return PerClassCapacity * 5; }
  static constexpr std::array<std::uint8_t, 5> lane_weights() noexcept { return weights_; }
};
} // namespace christeye::wind
