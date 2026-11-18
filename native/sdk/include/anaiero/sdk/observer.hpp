#pragma once
#include "anaiero/sdk/engine.hpp"
#include <memory>

namespace anaiero::sdk {
enum class ObserverEventKind : std::uint32_t {committed=0,failed,recovered,retired,trial_completed};
struct ObserverEvent {
  ObserverEventKind kind=ObserverEventKind::committed;
  EngineIdentity identity;
  std::string operation_id;
  std::uint64_t revision=0,tick=0,generation=0;
  Phase phase=Phase::ready;
  Code result=Code::ok;
};
struct IObserverSink {
  virtual ~IObserverSink()=default;
  virtual Result enqueue(const ObserverEvent&)=0;
};
struct IObserver {
  virtual ~IObserver()=default;
  virtual Result observe(const ObserverEvent&)=0;
};
struct IObserverClock {
  virtual ~IObserverClock()=default;
  virtual std::uint64_t now_ns()const noexcept=0;
};
// Native adapter implemented in anaiero_module_host, not in the portable core.
class SteadyObserverClock final:public IObserverClock {
public:
  std::uint64_t now_ns()const noexcept override;
};
struct ObserverStatistics {
  std::uint64_t accepted=0,dropped=0,delivered=0,callback_faults=0,budget_overruns=0;
  std::size_t queued=0,enabled_observers=0;
};
// The owner only enqueues passive values. A host dispatches callbacks later,
// outside SessionService. Faults and elapsed-budget overruns are independent
// diagnostics and disable the offending observer. Inline code is not preempted.
class ObserverService final:public IObserverSink {
  struct Impl;std::unique_ptr<Impl> impl_;
public:
  explicit ObserverService(std::size_t event_capacity=64,std::size_t observer_capacity=8);
  ~ObserverService();
  Result add(std::string id,std::shared_ptr<IObserver>);
  Result enqueue(const ObserverEvent&)override;
  Result dispatch(std::size_t maximum_callbacks,std::uint64_t callback_budget_ns,const IObserverClock&);
  ObserverStatistics statistics()const noexcept;
};
}
