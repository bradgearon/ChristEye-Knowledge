#pragma once
// WindPacket transport adapter; never authenticates network/device identity.
// Wire conversion preserves exactly the typed contract; no native struct casts.
#include "anaiero/services/runtime_service.hpp"
#include "christeye/wind/packet.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
namespace anaiero::runtime {
namespace wire_detail {
inline void u16(std::span<std::byte> b,std::size_t off,std::uint16_t n) noexcept {
  b[off]=std::byte(n&255);b[off+1]=std::byte(n>>8);
}
inline void u32(std::span<std::byte> b,std::size_t off,std::uint32_t n) noexcept {
  for(unsigned i=0;i<4;++i)b[off+i]=std::byte((n>>(8*i))&255);
}
inline void u64(std::span<std::byte> b,std::size_t off,std::uint64_t n) noexcept {
  for(unsigned i=0;i<8;++i)b[off+i]=std::byte((n>>(8*i))&255);
}
inline std::uint16_t get16(std::span<const std::byte> b,std::size_t n) noexcept {
  return std::uint16_t(std::to_integer<std::uint8_t>(b[n])|
         (std::uint16_t(std::to_integer<std::uint8_t>(b[n+1]))<<8));
}
inline std::uint32_t get32(std::span<const std::byte> b,std::size_t n) noexcept {
  std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(std::to_integer<std::uint8_t>(b[n+i]))<<(8*i);return v;
}
inline std::uint64_t get64(std::span<const std::byte> b,std::size_t n) noexcept {
  std::uint64_t v=0;for(unsigned i=0;i<8;++i)v|=std::uint64_t(std::to_integer<std::uint8_t>(b[n+i]))<<(8*i);return v;
}
} // namespace wire_detail
inline constexpr std::size_t kObservationPayload=64;
inline constexpr std::size_t kObservationFrame=christeye::wind::kFixedHeaderBytes+kObservationPayload;
inline bool encode_observation(const Observation& o,std::span<std::byte> output,
                               std::size_t& written) noexcept {
  written=0;
  if(!valid_observation(o)||output.size()<kObservationFrame)return false;
  std::array<std::byte,kObservationPayload> payload{};
  wire_detail::u32(payload,0,o.colony);wire_detail::u32(payload,4,o.animon);
  wire_detail::u32(payload,8,o.source);wire_detail::u32(payload,12,o.clock_domain);
  wire_detail::u32(payload,16,static_cast<std::uint32_t>(o.metric));
  wire_detail::u32(payload,20,static_cast<std::uint32_t>(o.lane));
  wire_detail::u32(payload,24,static_cast<std::uint32_t>(o.origin));
  wire_detail::u32(payload,28,std::bit_cast<std::uint32_t>(o.value));
  wire_detail::u16(payload,32,o.urgency);wire_detail::u16(payload,34,o.importance);
  wire_detail::u64(payload,36,o.sequence);wire_detail::u64(payload,44,o.acquired_ns);
  wire_detail::u64(payload,52,o.evidence_tag);
  christeye::wind::Header h{};
  h.kind=christeye::wind::Kind::observation;
  h.delivery_class=o.lane==Lane::fault?christeye::wind::DeliveryClass::fault:
    o.lane==Lane::desire?christeye::wind::DeliveryClass::decision:
    o.lane==Lane::evidence?christeye::wind::DeliveryClass::evidence:
    o.lane==Lane::telemetry?christeye::wind::DeliveryClass::status:
    christeye::wind::DeliveryClass::acquisition;
  h.source_slot=o.source;h.sequence=o.sequence;h.stream_id=o.colony;
  h.schema_id=kObservationSchema;h.payload_bytes=kObservationPayload;
  return christeye::wind::encode(h,{},payload,output,written)==christeye::wind::Error::ok;
}
inline Status decode_observation(std::span<const std::byte> packet,Observation& result) noexcept {
  christeye::wind::View v{};
  if(christeye::wind::decode(packet,v)!=christeye::wind::Error::ok||
     v.header.kind!=christeye::wind::Kind::observation||
     v.header.schema_id!=kObservationSchema||v.payload.size()!=kObservationPayload||
     wire_detail::get32(v.payload,60)!=0)return Status::malformed_wire;
  const auto b=v.payload;
  Observation o{};
  o.colony=wire_detail::get32(b,0);o.animon=wire_detail::get32(b,4);
  o.source=wire_detail::get32(b,8);o.clock_domain=wire_detail::get32(b,12);
  o.metric=static_cast<Metric>(wire_detail::get32(b,16));
  o.lane=static_cast<Lane>(wire_detail::get32(b,20));
  o.origin=static_cast<Origin>(wire_detail::get32(b,24));
  o.value=std::bit_cast<std::int32_t>(wire_detail::get32(b,28));
  o.urgency=wire_detail::get16(b,32);o.importance=wire_detail::get16(b,34);
  o.sequence=wire_detail::get64(b,36);o.acquired_ns=wire_detail::get64(b,44);
  o.evidence_tag=wire_detail::get64(b,52);
  const auto expected=o.lane==Lane::fault?christeye::wind::DeliveryClass::fault:
    o.lane==Lane::desire?christeye::wind::DeliveryClass::decision:
    o.lane==Lane::evidence?christeye::wind::DeliveryClass::evidence:
    o.lane==Lane::telemetry?christeye::wind::DeliveryClass::status:
    christeye::wind::DeliveryClass::acquisition;
  if(!valid_observation(o)||v.header.source_slot!=o.source||
     v.header.stream_id!=o.colony||v.header.sequence!=o.sequence||
     v.header.delivery_class!=expected)return Status::malformed_wire;
  result=o;return Status::ok;
}
inline Status ingress_frame(RuntimeService& runtime,std::span<const std::byte> packet) noexcept {
  Observation observation{};
  const auto r=decode_observation(packet,observation);
  return r==Status::ok?runtime.post(observation):r;
}
} // namespace anaiero::runtime
