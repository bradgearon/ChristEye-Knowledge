#pragma once
// Nonblocking wake coalescer; trials run later on an application work budget.
#include "anaiero/contracts/runtime_events.hpp"
#include <atomic>
namespace anaiero::optimization {
class OptimizationTriggerService final : public runtime::IUpdateObserver {
  std::atomic<std::uint64_t> pending_{0};
  void (*wakeup_)(void*) noexcept=nullptr;
  void* context_=nullptr;
public:
  void set_wakeup(void (*fn)(void*) noexcept,void* ctx) noexcept {
    wakeup_=fn;context_=ctx;
  }
  void on_revision(std::uint64_t revision,const runtime::Observation& e) noexcept override {
    if(e.origin!=runtime::Origin::simulated||
       (e.metric!=runtime::Metric::digital_learning&&
        e.metric!=runtime::Metric::connection))return;
    pending_.store(revision,std::memory_order_release);
    if(wakeup_)wakeup_(context_);
  }
  std::uint64_t take_pending_revision() noexcept {
    return pending_.exchange(0,std::memory_order_acq_rel);
  }
};
} // namespace anaiero::optimization
