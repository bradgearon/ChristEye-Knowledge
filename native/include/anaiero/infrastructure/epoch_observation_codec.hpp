#pragma once
// New schema does not change the legacy 120-byte Observation WindPacket/1.
// Epoch bytes are explicit, never silently dropped when using the v1 codec.
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/infrastructure/windbus_adapter.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
namespace anaiero::runtime {
class EpochObservationCodec final {
  static christeye::wind::DeliveryClass delivery(Lane lane) noexcept {
    using D=christeye::wind::DeliveryClass;
    switch(lane){
      case Lane::fault:return D::fault;
      case Lane::health:return D::acquisition;
      case Lane::desire:return D::decision;
      case Lane::evidence:return D::evidence;
      case Lane::telemetry:return D::status;
    }
    return D::status;
  }
public:
  bool encode(const Observation& o,
    std::array<std::byte,kEpochObservationFrame>& out) const noexcept {
    out={};
    if(!valid_observation(o)||o.source_epoch==0)return false;
    std::array<std::byte,kEpochObservationPayload> payload{};
    wire_detail::u32(payload,0,o.colony);
    wire_detail::u32(payload,4,o.animon);
    wire_detail::u32(payload,8,o.source);
    wire_detail::u32(payload,12,o.clock_domain);
    wire_detail::u32(payload,16,static_cast<std::uint32_t>(o.metric));
    wire_detail::u32(payload,20,static_cast<std::uint32_t>(o.lane));
    wire_detail::u32(payload,24,static_cast<std::uint32_t>(o.origin));
    wire_detail::u32(payload,28,std::bit_cast<std::uint32_t>(o.value));
    wire_detail::u16(payload,32,o.urgency);
    wire_detail::u16(payload,34,o.importance);
    wire_detail::u64(payload,36,o.sequence);
    wire_detail::u64(payload,44,o.acquired_ns);
    wire_detail::u64(payload,52,o.evidence_tag);
    // Bytes 60..63 remain reserved and must always be zero.
    wire_detail::u64(payload,64,o.source_epoch);
    christeye::wind::Header h{};
    h.kind=christeye::wind::Kind::observation;
    h.delivery_class=delivery(o.lane);
    h.source_slot=o.source;
    h.stream_id=o.colony;
    h.sequence=o.sequence;
    h.schema_id=kEpochObservationSchema;
    h.payload_bytes=kEpochObservationPayload;
    std::size_t length=0;
    return christeye::wind::encode(h,{},payload,out,length)==
      christeye::wind::Error::ok&&length==out.size();
  }
  bool decode(std::span<const std::byte> wire,Observation& result) const noexcept {
    christeye::wind::View v{};
    if(christeye::wind::decode(wire,v)!=christeye::wind::Error::ok||
       v.header.kind!=christeye::wind::Kind::observation||
       v.header.schema_id!=kEpochObservationSchema||
       v.header.header_bytes!=christeye::wind::kFixedHeaderBytes||
       v.payload.size()!=kEpochObservationPayload||
       wire_detail::get32(v.payload,60)!=0)return false;
    Observation candidate{};
    candidate.colony=wire_detail::get32(v.payload,0);
    candidate.animon=wire_detail::get32(v.payload,4);
    candidate.source=wire_detail::get32(v.payload,8);
    candidate.clock_domain=wire_detail::get32(v.payload,12);
    candidate.metric=static_cast<Metric>(wire_detail::get32(v.payload,16));
    candidate.lane=static_cast<Lane>(wire_detail::get32(v.payload,20));
    candidate.origin=static_cast<Origin>(wire_detail::get32(v.payload,24));
    candidate.value=std::bit_cast<std::int32_t>(wire_detail::get32(v.payload,28));
    candidate.urgency=wire_detail::get16(v.payload,32);
    candidate.importance=wire_detail::get16(v.payload,34);
    candidate.sequence=wire_detail::get64(v.payload,36);
    candidate.acquired_ns=wire_detail::get64(v.payload,44);
    candidate.evidence_tag=wire_detail::get64(v.payload,52);
    candidate.source_epoch=wire_detail::get64(v.payload,64);
    if(!valid_observation(candidate)||candidate.source_epoch==0||
       v.header.source_slot!=candidate.source||
       v.header.stream_id!=candidate.colony||
       v.header.sequence!=candidate.sequence||
       v.header.delivery_class!=delivery(candidate.lane)||
       v.header.destination_slot!=0)return false;
    result=candidate;return true;
  }
};
} // namespace anaiero::runtime
