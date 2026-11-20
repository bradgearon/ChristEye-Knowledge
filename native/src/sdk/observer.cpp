#include "anaiero/sdk/observer.hpp"
#include <deque>

namespace anaiero::sdk {
struct ObserverService::Impl {
  struct Subscriber {std::string id;std::shared_ptr<IObserver> observer;bool enabled=true;};
  struct Pending {ObserverEvent event;std::size_t next=0;};
  std::size_t event_capacity,observer_capacity;std::vector<Subscriber> subscribers;std::deque<Pending> events;
  ObserverStatistics stats;bool frozen=false,dispatching=false;
  Impl(std::size_t events_max,std::size_t observers_max):event_capacity(events_max),observer_capacity(observers_max){}
};
ObserverService::ObserverService(std::size_t events,std::size_t observers):impl_(std::make_unique<Impl>(events,observers)){}
ObserverService::~ObserverService()=default;
Result ObserverService::add(std::string id,std::shared_ptr<IObserver> observer) {
  if(impl_->frozen||impl_->dispatching)return {Code::conflict,"observer registration is frozen"};
  if(!valid_token(id)||!observer)return {Code::invalid,"observer registration"};
  for(const auto& existing:impl_->subscribers)if(existing.id==id)return {Code::conflict,"duplicate observer identity"};
  if(!impl_->observer_capacity||impl_->observer_capacity>64||impl_->subscribers.size()==impl_->observer_capacity)
    return {Code::capacity,"observer registration capacity"};
  impl_->subscribers.push_back({std::move(id),std::move(observer),true});return {};
}
Result ObserverService::enqueue(const ObserverEvent& event) {
  impl_->frozen=true;
  if(!valid_identity(event.identity)||!valid_token(event.operation_id))return {Code::invalid,"observer event identity"};
  if(impl_->dispatching)return {Code::busy,"observer cannot reenter its event queue"};
  if(!impl_->event_capacity||impl_->event_capacity>65536||impl_->events.size()==impl_->event_capacity) {
    ++impl_->stats.dropped;return {Code::capacity,"observer event queue full"};
  }
  impl_->events.push_back({event,0});++impl_->stats.accepted;return {};
}
Result ObserverService::dispatch(std::size_t maximum,std::uint64_t budget,const IObserverClock& clock) {
  if(impl_->dispatching)return {Code::busy,"observer dispatch is not reentrant"};
  if(!maximum||maximum>4096||!budget)return {Code::invalid,"observer dispatch budget"};
  impl_->frozen=true;impl_->dispatching=true;
  struct Reset {bool& value;~Reset(){value=false;}} reset{impl_->dispatching};
  std::size_t delivered=0;
  while(!impl_->events.empty()&&delivered<maximum) {
    auto& event=impl_->events.front();
    if(event.next==impl_->subscribers.size()){impl_->events.pop_front();continue;}
    auto& subscriber=impl_->subscribers[event.next++];if(!subscriber.enabled)continue;
    const auto start=clock.now_ns();Result result;
    try{result=subscriber.observer->observe(event.event);}catch(...){result={Code::io_error,"observer callback threw"};}
    const auto finish=clock.now_ns();++delivered;++impl_->stats.delivered;
    if(!result){++impl_->stats.callback_faults;subscriber.enabled=false;}
    if(finish<start||finish-start>budget){++impl_->stats.budget_overruns;subscriber.enabled=false;}
  }
  while(!impl_->events.empty()&&impl_->events.front().next==impl_->subscribers.size())impl_->events.pop_front();
  return {};
}
ObserverStatistics ObserverService::statistics()const noexcept {
  auto result=impl_->stats;result.queued=impl_->events.size();
  for(const auto& subscriber:impl_->subscribers)if(subscriber.enabled)++result.enabled_observers;
  return result;
}
}
