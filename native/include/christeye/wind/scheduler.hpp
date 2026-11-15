#pragma once
// Bounded, allocation-free, single-owner weighted endpoint scheduler.
// A transport callback/ISR-notified task calls post(); a notified consumer
// drains next() until empty. No timed queue polling or hidden worker thread.
// Not thread-safe: platform adapters must serialize ownership explicitly.
#include <array>
#include <cstddef>
#include <cstdint>
#include "christeye/wind/packet.hpp"

namespace christeye::wind {
struct WorkItem {
  std::uint64_t token = 0;          // caller-owned immutable payload descriptor
  std::uint64_t deadline_ns = 0;    // 0 means no deadline; monotonic clock domain
};

enum class PostResult : std::uint8_t { accepted, full, invalid_class };
enum class TakeStatus : std::uint8_t { empty, ready, expired };
struct TakeResult { TakeStatus status = TakeStatus::empty; WorkItem item{};
                    DeliveryClass delivery_class = DeliveryClass::status; };

// Fixed weights are an INITIAL scheduling policy, not scientific evidence
// scores and never an authorization decision. Each class has separate bounded
// storage so UI floods cannot consume the fault lane's slots.
template<std::size_t PerClassCapacity>
class BoundedScheduler {
  static_assert(PerClassCapacity > 0, "nonzero per-class capacity required");
  static constexpr std::array<std::uint8_t, 5> weights_{{8, 6, 4, 3, 1}};
  static constexpr std::size_t schedule_size_ = 22;
  struct Ring {
    std::array<WorkItem, PerClassCapacity> slots{};
    std::size_t read = 0, write = 0, count = 0;
    bool push(WorkItem v) noexcept {
      if (count == PerClassCapacity) return false;
      slots[write] = v; write = (write + 1) % PerClassCapacity; ++count; return true;
    }
    WorkItem pop() noexcept {
      const auto v = slots[read]; read = (read + 1) % PerClassCapacity; --count; return v;
    }
  };
  std::array<Ring, 5> rings_{};
  std::size_t count_ = 0, cursor_ = 0;
  void (*notify_)(void*) = nullptr;
  void* notify_context_ = nullptr;

  static constexpr std::array<std::uint8_t, schedule_size_> make_schedule() noexcept {
    std::array<std::uint8_t, schedule_size_> out{};
    std::size_t index = 0;
    for (std::uint8_t cls = 0; cls < weights_.size(); ++cls)
      for (std::uint8_t i = 0; i < weights_[cls]; ++i) out[index++] = cls;
    return out;
  }
  static constexpr auto schedule_ = make_schedule();
 public:
  explicit BoundedScheduler(void (*notify)(void*) = nullptr, void* ctx = nullptr) noexcept
      : notify_(notify), notify_context_(ctx) {}
  BoundedScheduler(const BoundedScheduler&) = delete;
  BoundedScheduler& operator=(const BoundedScheduler&) = delete;

  PostResult post(DeliveryClass cls, WorkItem item) noexcept {
    const auto idx = std::uint8_t(cls);
    if (idx >= rings_.size()) return PostResult::invalid_class;
    const bool was_empty = count_ == 0;
    if (!rings_[idx].push(item)) return PostResult::full;
    ++count_;
    if (was_empty && notify_) notify_(notify_context_);
    return PostResult::accepted;
  }
  TakeResult next(std::uint64_t now_ns) noexcept {
    if (count_ == 0) return {};
    // Every nonempty lane has at least one slot in the weighted schedule.
    for (std::size_t attempts = 0; attempts < schedule_.size(); ++attempts) {
      const std::size_t idx = schedule_[cursor_];
      cursor_ = (cursor_ + 1) % schedule_.size();
      if (rings_[idx].count == 0) continue;
      const auto v = rings_[idx].pop(); --count_;
      const bool expired = v.deadline_ns != 0 && now_ns >= v.deadline_ns;
      return {expired ? TakeStatus::expired : TakeStatus::ready, v, DeliveryClass(idx)};
    }
    return {}; // unreachable unless internal state corrupted
  }
  std::size_t pending() const noexcept { return count_; }
  std::size_t pending(DeliveryClass cls) const noexcept {
    const auto idx = std::uint8_t(cls);
    return idx < rings_.size() ? rings_[idx].count : 0;
  }
  static constexpr std::size_t capacity() noexcept { return 5 * PerClassCapacity; }
  static constexpr std::array<std::uint8_t, 5> lane_weights() noexcept { return weights_; }
};
}  // namespace christeye::wind
