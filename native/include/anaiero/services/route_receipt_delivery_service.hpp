#pragma once
// SW6.1: service-owned journal -> WindBus -> projector -> outbox ACK ordering.
// A sink acknowledgement is for *local durable receipt + materialization*, not
// durable UI delivery, mesh authorization, or real-world effects.
#include "anaiero/contracts/route_receipt_delivery.hpp"
#include <cstdint>
#include <string_view>

namespace anaiero::optimization {
inline bool same_route_receipt(const RouteReceipt& a,const RouteReceipt& b) noexcept {
  return a.kind==b.kind&&a.sequence==b.sequence&&a.change_id==b.change_id&&
    a.revision==b.revision&&a.state_sha256==b.state_sha256&&
    a.candidate.id==b.candidate.id&&a.candidate.colony==b.candidate.colony&&
    a.candidate.from==b.candidate.from&&a.candidate.to==b.candidate.to&&
    a.candidate.kind==b.candidate.kind&&
    a.candidate.digital_cost==b.candidate.digital_cost&&
    a.candidate.model_risk==b.candidate.model_risk&&
    a.candidate.guidance_weight==b.candidate.guidance_weight;
}
class RouteReceiptDeliveryService final:public IRouteReceiptSink,private IRouteReceiptReplay {
  IRouteReceiptCodec& codec_;
  IRouteReceiptJournal& journal_;
  IRouteReceiptProjection& projector_;
  std::uint32_t source_=0;
  std::uint64_t last_sequence_=0;
  RouteReceipt latest_{};
  bool initialized_=false;
  bool on_frame(std::span<const std::byte> data) noexcept override {
    RouteReceipt receipt{};
    if(!codec_.decode(data,source_,receipt)||
       receipt.sequence!=last_sequence_+1||
       !projector_.project(receipt))return false;
    last_sequence_=receipt.sequence;latest_=std::move(receipt);
    return true;
  }
public:
  RouteReceiptDeliveryService(IRouteReceiptCodec& codec,IRouteReceiptJournal& journal,
    IRouteReceiptProjection& projector,std::uint32_t source) noexcept:
    codec_(codec),journal_(journal),projector_(projector),source_(source){}
  bool recover() noexcept {
    if(initialized_||!source_||!projector_.initialize())return false;
    if(!journal_.replay(*this))return false;
    initialized_=true;return true;
  }
  bool publish(const RouteReceipt& receipt) noexcept override {
    if(!initialized_)return false;
    // Called again after a ledger ACK failure: the same receipt must be
    // accepted, without duplicating journal or projector revisions.
    if(receipt.sequence==last_sequence_)return same_route_receipt(receipt,latest_);
    if(receipt.sequence!=last_sequence_+1)return false;
    std::array<std::byte,kRouteReceiptPacketBytes> frame{};
    if(!codec_.encode(receipt,source_,frame)||!journal_.append(receipt,frame)||
       !projector_.project(receipt))return false;
    last_sequence_=receipt.sequence;latest_=receipt;
    return true;
  }
  std::uint64_t acknowledged_sequence()const noexcept{return last_sequence_;}
};
} // namespace anaiero::optimization
