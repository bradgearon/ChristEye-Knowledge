#pragma once
// Map a versioned digital route receipt to the existing SW4 volatile state.
// Movement goes through the typed WindPacket observation adapter, never
// directly into the passive ColonyState model.
#include "anaiero/contracts/route_receipt_delivery.hpp"
#include "anaiero/infrastructure/windbus_adapter.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace anaiero::optimization {
class RouteRuntimeProjectionPort final:public IRouteReceiptProjection {
  runtime::RuntimeService& runtime_;
  std::uint32_t source_,colony_,clock_domain_;
  bool initialized_=false;
  static std::uint64_t evidence_tag(const std::string& sha) noexcept {
    std::uint64_t result=0;
    for(unsigned i=0;i<16;++i) {
      const char c=sha[i];
      const auto digit=static_cast<std::uint64_t>(c<='9'?c-'0':c-'a'+10);
      result=(result<<4)|digit;
    }
    return result;
  }
  bool projected(const runtime::Observation& o) const noexcept {
    const auto state=runtime_.latest(o.acquired_ns,std::numeric_limits<std::uint64_t>::max());
    for(std::size_t i=0;i<state.count;++i) {
      const auto& v=state.values[i];
      if(v.colony==o.colony&&v.animon==o.animon&&v.source==o.source&&
         v.metric==o.metric&&v.sequence==o.sequence&&v.value==o.value&&
         v.evidence_tag==o.evidence_tag&&v.origin==runtime::Origin::simulated&&
         v.quality==runtime::Quality::current)return true;
    }
    return false;
  }
public:
  RouteRuntimeProjectionPort(runtime::RuntimeService& runtime,
    std::uint32_t source,std::uint32_t colony,std::uint32_t clock_domain=1) noexcept:
    runtime_(runtime),source_(source),colony_(colony),clock_domain_(clock_domain){}
  bool initialize() noexcept override {
    if(initialized_||!source_||!colony_||!clock_domain_)return false;
    if(runtime_.enroll({source_,clock_domain_,runtime::Origin::simulated,
                       false,false,false})!=runtime::Status::ok)return false;
    initialized_=true;return true;
  }
  bool project(const RouteReceipt& receipt) noexcept override {
    if(!initialized_||receipt.candidate.colony!=colony_||
       receipt.kind==RouteEventKind::none||receipt.state_sha256.size()!=64)return false;
    runtime::Observation o{};
    o.colony=colony_;o.animon=receipt.candidate.id;
    o.source=source_;o.clock_domain=clock_domain_;
    o.metric=runtime::Metric::digital_route_active;
    o.lane=runtime::Lane::evidence;o.origin=runtime::Origin::simulated;
    o.value=receipt.kind==RouteEventKind::route_reverted?0:1;
    o.sequence=receipt.sequence;o.acquired_ns=receipt.sequence*1000000ULL;
    o.evidence_tag=evidence_tag(receipt.state_sha256);
    o.urgency=150;o.importance=200;
    if(projected(o))return true;
    std::array<std::byte,runtime::kObservationFrame> packet{};
    std::size_t written=0;
    if(!runtime::encode_observation(o,packet,written))return false;
    if(runtime::ingress_frame(runtime_,std::span<const std::byte>(packet.data(),written))
       !=runtime::Status::ok)return false;
    // Local projection ACK is only returned after the expected source
    // sequence and value are visible, not after mere queue admission.
    for(unsigned i=0;i<4&&runtime_.pending();++i)runtime_.drain(64);
    return projected(o);
  }
};
} // namespace anaiero::optimization
