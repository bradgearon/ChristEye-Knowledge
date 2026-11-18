#pragma once
// Single-owner durable replay and backpressure. The gateway rechecks its
// verified session before invoking any method; packet claims are not identity.
#include "anaiero/contracts/mesh_replay.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <array>
#include <cstddef>
#include <string_view>

namespace anaiero::mesh {
class MeshReplayService final {
  struct Slot {
    bool used=false,offered=false;
    ReplaySubscription subscription{};
    std::uint64_t generation=0;
    ReplayCursor acknowledged{};
    ReplayPage offer{};
  };
  IMeshReplayStore& store_;
  std::array<Slot,kMaxReplaySubscriptions> slots_{};
  contracts::IServiceExtension* extension_=nullptr;
  Slot* find(const AuthorizedPeer& peer,std::string_view id)noexcept {
    for(auto& slot:slots_)if(slot.used&&slot.subscription.principal==peer.principal&&slot.subscription.id==id)return &slot;
    return nullptr;
  }
  static bool valid_peer(const AuthorizedPeer& peer)noexcept{return peer.generation&&mesh_token(peer.principal);}
  void emit(const Slot& slot,contracts::ServicePhase phase)noexcept {
    if(extension_)extension_->on_service_event({contracts::ServiceId::mesh_replay,phase,
      slot.acknowledged.sequence,slot.subscription.scope.colony,0,0,slot.subscription.scope.epoch,slot.subscription.scope.source});
  }
public:
  explicit MeshReplayService(IMeshReplayStore& store)noexcept:store_(store){}
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  ReplayCode open(const AuthorizedPeer& peer,std::string_view id,const Scope& scope,ReplayCursor& cursor)noexcept {
    if(!valid_peer(peer)||!mesh_token(id)||!valid_scope(scope))return ReplayCode::invalid;
    ReplaySubscription subscription{peer.principal,std::string(id),scope};
    auto* slot=find(peer,id);
    if(slot&&slot->subscription!=subscription)return ReplayCode::conflict;
    if(!slot)for(auto& available:slots_)if(!available.used){slot=&available;break;}
    if(!slot)return ReplayCode::capacity;
    ReplayCursor durable;const auto status=store_.open_subscription(subscription,durable);
    if(status!=ReplayCode::ok)return status;
    if(!valid_replay_cursor(durable))return ReplayCode::corrupt;
    *slot={true,false,std::move(subscription),peer.generation,durable,{}};
    cursor=durable;emit(*slot,contracts::ServicePhase::proposed);return ReplayCode::ok;
  }
  ReplayCode offer(const AuthorizedPeer& peer,std::string_view id,ReplayPage& page)noexcept {
    if(!valid_peer(peer)||!mesh_token(id))return ReplayCode::invalid;
    auto* slot=find(peer,id);if(!slot)return ReplayCode::not_found;
    if(slot->generation!=peer.generation)return ReplayCode::denied;
    if(slot->offered){page=slot->offer;return ReplayCode::ok;}
    ReplayPage next;const auto status=store_.read_page(slot->subscription.scope,slot->acknowledged,next);
    if(status!=ReplayCode::ok)return status;
    if(next.after!=slot->acknowledged||!valid_replay_cursor(next.terminal)||
      next.terminal.sequence<next.after.sequence||next.count>kMaxReplayRecords||next.examined>kMaxReplayExamined||
      next.count>next.examined||next.terminal.sequence-next.after.sequence!=next.examined||
      (next.examined==0&&(next.terminal!=next.after||!next.caught_up)))return ReplayCode::corrupt;
    std::uint64_t prior=next.after.sequence;
    for(std::size_t i=0;i<next.count;++i){const auto& record=next.records[i];
      if(!valid_replay_cursor(record.cursor)||record.cursor.sequence<=prior||record.cursor.sequence>next.terminal.sequence||
        (record.cursor.sequence==next.terminal.sequence&&record.cursor.hash!=next.terminal.hash)||
        !scope_matches(slot->subscription.scope,record.observation)||!runtime::valid_observation(record.observation))return ReplayCode::corrupt;
      prior=record.cursor.sequence;
    }
    // An empty caught-up response has no outstanding delivery to acknowledge.
    // Do not cache it indefinitely: a later request may observe newly committed
    // events without requiring a disconnect or synthetic acknowledgement.
    slot->offer=next;slot->offered=next.examined!=0;page=std::move(next);return ReplayCode::ok;
  }
  ReplayCode acknowledge(const AuthorizedPeer& peer,std::string_view id,const ReplayCursor& cursor)noexcept {
    if(!valid_peer(peer)||!mesh_token(id)||!valid_replay_cursor(cursor))return ReplayCode::invalid;
    auto* slot=find(peer,id);if(!slot)return ReplayCode::not_found;
    if(slot->generation!=peer.generation)return ReplayCode::denied;
    if(cursor==slot->acknowledged)return ReplayCode::ok;
    if(!slot->offered||cursor!=slot->offer.terminal)return ReplayCode::conflict;
    const auto status=store_.acknowledge(slot->subscription,cursor);
    if(status!=ReplayCode::ok)return status;
    slot->acknowledged=cursor;slot->offered=false;slot->offer={};emit(*slot,contracts::ServicePhase::committed);
    return ReplayCode::ok;
  }
  ReplayCode scope(const AuthorizedPeer& peer,std::string_view id,Scope& output)const noexcept {
    if(!valid_peer(peer)||!mesh_token(id))return ReplayCode::invalid;
    for(const auto& slot:slots_)if(slot.used&&slot.subscription.principal==peer.principal&&slot.subscription.id==id){
      if(slot.generation!=peer.generation)return ReplayCode::denied;
      output=slot.subscription.scope;return ReplayCode::ok;
    }
    return ReplayCode::not_found;
  }
  void disconnect(const AuthorizedPeer& peer)noexcept {
    for(auto& slot:slots_)if(slot.used&&slot.subscription.principal==peer.principal&&slot.generation==peer.generation){
      slot={};
    }
  }
};
} // namespace anaiero::mesh
