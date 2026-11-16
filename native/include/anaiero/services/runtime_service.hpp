#pragma once
// Digital-only coordinator: source policy, weighted dispatch, projector, sinks.
#include "anaiero/services/colony_projector.hpp"
#include "anaiero/infrastructure/weighted_queue.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
namespace anaiero::runtime {
class AnimonPriorityService {
public:
  // Scheduling priority is NOT physiological severity, diagnosis, physical
  // support entitlement, or a permission for biological intervention.
  static Observation classify(Observation o) noexcept {
    if(o.metric==Metric::digital_desire)o.lane=Lane::desire;
    else if(health_metric(o.metric)&&o.lane!=Lane::fault)o.lane=Lane::health;
    if(o.metric==Metric::viability&&o.value==0&&o.lane==Lane::health){
      // Raise a digital review event's queue importance, not a care command.
      if(o.urgency<800)o.urgency=800;
    }
    return o;
  }
};
class RuntimeService {
  WeightedQueue<> queue_{};
  ColonyProjector projector_{};
  std::array<SourcePermission,kMaxSources> permissions_{};
  std::size_t permissions_count_=0;
  IUpdateObserver* observer_=nullptr;
  IEventCommit* journal_=nullptr;
  std::optional<Observation> retry_{}; // one reserved in-flight slot, bounded
  void (*wakeup_)(void*) noexcept=nullptr;
  void* wake_context_=nullptr;
  std::uint64_t rejected_=0,committed_=0,commit_failures_=0;
  bool allowed(const Observation& o) const noexcept {
    for(std::size_t i=0;i<permissions_count_;++i){
      const auto& p=permissions_[i];
      if(p.source!=o.source)continue;
      if(p.origin!=o.origin||p.clock_domain!=o.clock_domain)return false;
      if(o.lane==Lane::fault&&!p.can_publish_fault)return false;
      if(health_metric(o.metric)&&!p.can_read_health)return false;
      if(o.metric==Metric::digital_desire&&(!p.can_publish_digital_desires||
          p.origin!=Origin::simulated))return false;
      if(o.origin==Origin::external_unverified_readonly&&o.metric==Metric::digital_learning)return false;
      return true;
    }
    return false;
  }
public:
  Status enroll(SourcePermission p) noexcept {
    if(!p.source||!p.clock_domain||!valid_origin(p.origin)||permissions_count_==kMaxSources)
      return Status::invalid;
    for(std::size_t i=0;i<permissions_count_;++i)
      if(permissions_[i].source==p.source)return Status::invalid;
    permissions_[permissions_count_++]=p;
    return Status::ok;
  }
  void set_observer(IUpdateObserver* listener) noexcept {observer_=listener;}
  void set_journal(IEventCommit* journal) noexcept {
    journal_=journal;
    if(retry_&&wakeup_)wakeup_(wake_context_);
  }
  void set_wakeup(void (*fn)(void*) noexcept,void* ctx) noexcept {
    wakeup_=fn;wake_context_=ctx;
  }
  Status post(Observation o) noexcept {
    o=AnimonPriorityService::classify(o);
    if(!valid_observation(o))return Status::invalid;
    if(!allowed(o))return Status::unauthorized;
    const bool empty=queue_.size()==0&&!retry_;
    auto status=queue_.push(o);
    if(status!=Status::ok){++rejected_;return status;}
    if(empty&&wakeup_)wakeup_(wake_context_);
    return Status::ok;
  }
  // Explicit bounded dispatch on the owning loop's event notification.
  // If persistence is configured, only committed events enter the projector.
  // Production ingress must authenticate peers BEFORE calling post().
  std::size_t drain(std::size_t budget=32) noexcept {
    std::size_t handled=0;
    Observation o;
    while(handled<budget){
      if(retry_){o=*retry_;retry_.reset();}
      else if(!queue_.take(o))break;
      ++handled;
      if(projector_.can_apply(o)!=Status::ok){++rejected_;continue;}
      if(journal_&&!journal_->commit(o)){
        retry_=o;
        ++commit_failures_;
        break;  // retain order; restart on a journal recovery/completion event
      }
      const auto status=projector_.apply(o);
      if(status!=Status::ok){++rejected_;continue;}
      ++committed_;
      if(observer_)observer_->on_revision(projector_.revision(),o);
    }
    return handled;
  }
  ColonyState latest(std::uint64_t now_ns,std::uint64_t ttl_ns) const noexcept {
    return projector_.snapshot(now_ns,ttl_ns);
  }
  std::size_t pending() const noexcept {return queue_.size()+(retry_?1u:0u);}
  std::uint64_t commit_failures() const noexcept {return commit_failures_;}
  std::uint64_t rejected() const noexcept{return rejected_;}
  std::uint64_t projected() const noexcept{return committed_;}
};
} // namespace anaiero::runtime
