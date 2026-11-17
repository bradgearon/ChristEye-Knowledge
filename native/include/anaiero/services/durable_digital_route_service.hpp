#pragma once
// Application-facing digital route service; atomic ledger via injected port.
#include "anaiero/contracts/fork_optimization.hpp"
#include "anaiero/services/digital_route_policy_service.hpp"
#include <cstddef>
#include <cstdint>
namespace anaiero::optimization {
class DurableDigitalRouteService final:public IRouteChangePort {
  IAtomicRouteLedger& durable_;
  DigitalRoutePolicyService policy_;
  cells::Atlas atlas_{};
  StoreResult update(DigitalRouteLedger& old,DigitalRouteLedger& next) noexcept {
    next.revision=old.revision+1;
    next.atlas_hash=old.atlas_hash;
    next.initial_world=old.initial_world;
    next.observed_revision=old.observed_revision;
    return durable_.compare_exchange(old,next);
  }
public:
  DurableDigitalRouteService(IAtomicRouteLedger& storage,cells::Atlas atlas,
                             cells::ISpatialPort& spatial) noexcept
    :durable_(storage),policy_(atlas,spatial),atlas_(std::move(atlas)){}
  StoreResult initialize(const cells::World& source,std::uint64_t revision,
                         DigitalRouteLedger& out) noexcept {
    DigitalRouteLedger seed{};seed.atlas_hash=atlas_.model_digest;
    seed.observed_revision=revision;seed.initial_world=source;
    seed.current_world=source;seed.event_acked=true;
    return durable_.initialize(seed,out);
  }
  StoreResult read(DigitalRouteLedger& out) noexcept{return durable_.read(out);}
  bool commit(const Decision& decision,std::uint64_t& change_id) noexcept override {
    change_id=0;
    DigitalRouteLedger old{};
    if(durable_.read(old)!=StoreResult::ok||old.active||!old.event_acked||
       decision.status!=Status::ok||decision.score<=0||
       decision.snapshot_revision!=old.observed_revision||
       decision.source_world_sha256!=old.current_world_sha256||
       decision.selected.kind!=RouteKind::digital_signal||
       !decision.selected.guidance_weight)return false;
    ForkOption option{decision.selected,decision.selected.guidance_weight,0};
    DigitalRouteLedger next=old;
    if(!policy_.apply(old.current_world,option,next.current_world,
                      next.prior_existed,next.prior_guidance))return false;
    next.change_id=old.event_sequence+1;
    next.event_sequence=old.event_sequence+1;
    next.active=true;next.selected=decision.selected;
    next.applied_guidance=decision.selected.guidance_weight;
    next.event_kind=RouteEventKind::route_committed;next.event_acked=false;
    if(update(old,next)!=StoreResult::ok)return false;
    change_id=next.change_id;return true;
  }
  bool commit_selected(const ForkSelection& selection,std::uint64_t& change_id) noexcept {
    change_id=0;
    if(selection.status!=ForkStatus::ok||selection.score<=0)return false;
    Decision decision{};decision.status=Status::ok;decision.score=selection.score;
    decision.snapshot_revision=selection.snapshot_revision;
    decision.source_world_sha256=selection.initial_world_sha256;
    decision.selected=selection.option.route;
    return commit(decision,change_id);
  }
  bool revert(std::uint64_t change_id) noexcept override {
    DigitalRouteLedger old{};
    if(durable_.read(old)!=StoreResult::ok||!old.active||
       old.change_id!=change_id||!old.event_acked)return false;
    DigitalRouteLedger next=old;
    if(!policy_.undo(old.current_world,old.selected,old.prior_existed,
                     old.prior_guidance,next.current_world))return false;
    next.active=false;next.event_kind=RouteEventKind::route_reverted;
    next.event_acked=false;next.event_sequence=old.event_sequence+1;
    return update(old,next)==StoreResult::ok;
  }
  // Advancing a committed digital route is a separate, durable digital event.
  // It does not mutate the source .cevcells file or a physical preparation.
  bool advance(const cells::Context& context,std::uint16_t ticks,
               cells::ISpatialPort& spatial) noexcept {
    if(!ticks||ticks>128)return false;
    DigitalRouteLedger old{};
    if(durable_.read(old)!=StoreResult::ok||!old.active||!old.event_acked)return false;
    cells::CellBehaviorService engine(atlas_,spatial);
    if(engine.restore(old.current_world)!=cells::Status::ok)return false;
    for(std::uint16_t i=0;i<ticks;++i)
      if(engine.step(context)!=cells::Status::ok)return false;
    DigitalRouteLedger next=old;
    next.current_world=engine.world();
    next.event_sequence=old.event_sequence+1;
    next.event_kind=RouteEventKind::simulation_advanced;next.event_acked=false;
    return update(old,next)==StoreResult::ok;
  }
  // Outbox receipt remains pending until downstream confirms the event ID.
  // A crash between publish and ack replays the same (revision,sequence).
  bool deliver(IRouteReceiptSink& sink) noexcept {
    DigitalRouteLedger old{};
    if(durable_.read(old)!=StoreResult::ok)return false;
    if(old.event_acked)return true;
    const RouteReceipt receipt{old.event_kind,old.event_sequence,old.change_id,
      old.revision,old.selected,old.current_world_sha256};
    if(!sink.publish(receipt))return false;
    DigitalRouteLedger next=old;next.event_acked=true;
    return update(old,next)==StoreResult::ok;
  }
};
} // namespace anaiero::optimization
