#pragma once
// Exact 152-byte WindPacket/1 route receipt. The SHA identifies a digital
// checkpoint, not an authenticated sender or biological observation.
#include "anaiero/contracts/route_receipt_delivery.hpp"
#include "christeye/wind/packet.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace anaiero::optimization {
inline constexpr std::uint32_t kRouteReceiptSchema=0xCE600031u;
namespace route_wire {
inline void put32(std::span<std::byte> b,std::size_t at,std::uint32_t n) noexcept {
  for(unsigned i=0;i<4;++i)b[at+i]=std::byte((n>>(8*i))&255u);
}
inline void put64(std::span<std::byte> b,std::size_t at,std::uint64_t n) noexcept {
  for(unsigned i=0;i<8;++i)b[at+i]=std::byte((n>>(8*i))&255u);
}
inline std::uint32_t get32(std::span<const std::byte> b,std::size_t at) noexcept {
  std::uint32_t n=0;
  for(unsigned i=0;i<4;++i)n|=std::uint32_t(std::to_integer<std::uint8_t>(b[at+i]))<<(i*8);
  return n;
}
inline std::uint64_t get64(std::span<const std::byte> b,std::size_t at) noexcept {
  std::uint64_t n=0;
  for(unsigned i=0;i<8;++i)n|=std::uint64_t(std::to_integer<std::uint8_t>(b[at+i]))<<(i*8);
  return n;
}
inline unsigned hexval(char c) noexcept {
  if(c>='0'&&c<='9')return static_cast<unsigned>(c-'0');
  if(c>='a'&&c<='f')return static_cast<unsigned>(c-'a'+10);
  return 256;
}
inline bool digest(std::string_view h) noexcept {
  if(h.size()!=64)return false;
  for(const char c:h)if(hexval(c)>15)return false;
  return true;
}
inline bool valid(const RouteReceipt& r) noexcept {
  const auto kind=static_cast<unsigned>(r.kind);
  const auto type=static_cast<unsigned>(r.candidate.kind);
  return kind>=1&&kind<=3&&r.sequence>0&&r.sequence<=4096&&r.change_id>0&&
    r.revision>0&&r.revision<=4096&&r.candidate.id>0&&
    r.candidate.colony>0&&r.candidate.from>0&&r.candidate.to>0&&
    r.candidate.from!=r.candidate.to&&type>=1&&type<=3&&
    r.candidate.digital_cost<=1000&&r.candidate.model_risk<=1000&&
    r.candidate.guidance_weight<=1000&&digest(r.state_sha256);
}
} // namespace route_wire

class RouteReceiptWindPacketCodec final:public IRouteReceiptCodec {
public:
  bool encode(const RouteReceipt& r,std::uint32_t source,
    std::array<std::byte,kRouteReceiptPacketBytes>& frame) noexcept override {
    frame={};
    if(source==0||!route_wire::valid(r))return false;
    std::array<std::byte,kRouteReceiptPayloadBytes> payload{};
    route_wire::put32(payload,0,1); // payload contract version
    route_wire::put32(payload,4,static_cast<std::uint32_t>(r.kind));
    route_wire::put32(payload,8,r.candidate.colony);
    route_wire::put32(payload,12,r.candidate.id);
    route_wire::put32(payload,16,r.candidate.from);
    route_wire::put32(payload,20,r.candidate.to);
    route_wire::put32(payload,24,static_cast<std::uint32_t>(r.candidate.kind));
    route_wire::put32(payload,28,r.candidate.digital_cost);
    route_wire::put32(payload,32,r.candidate.model_risk);
    route_wire::put32(payload,36,r.candidate.guidance_weight);
    route_wire::put64(payload,40,r.sequence);
    route_wire::put64(payload,48,r.change_id);
    route_wire::put64(payload,56,r.revision);
    for(std::size_t i=0;i<32;++i) {
      const auto high=route_wire::hexval(r.state_sha256[i*2]);
      const auto low=route_wire::hexval(r.state_sha256[i*2+1]);
      payload[64+i]=std::byte((high<<4)|low);
    }
    christeye::wind::Header h{};
    h.kind=christeye::wind::Kind::change;
    h.delivery_class=christeye::wind::DeliveryClass::evidence;
    h.source_slot=source;
    h.stream_id=r.candidate.colony;
    h.sequence=r.sequence;
    h.schema_id=kRouteReceiptSchema;
    h.payload_bytes=kRouteReceiptPayloadBytes;
    std::size_t written=0;
    return christeye::wind::encode(h,{},payload,frame,written)==christeye::wind::Error::ok&&
      written==frame.size();
  }
  bool decode(std::span<const std::byte> frame,std::uint32_t source,
    RouteReceipt& out) noexcept override {
    christeye::wind::View view{};
    if(source==0||christeye::wind::decode(frame,view)!=christeye::wind::Error::ok||
      view.header.kind!=christeye::wind::Kind::change||
      view.header.delivery_class!=christeye::wind::DeliveryClass::evidence||
      view.header.schema_id!=kRouteReceiptSchema||view.header.source_slot!=source||
      view.header.destination_slot!=0||view.header.header_bytes!=christeye::wind::kFixedHeaderBytes||
      view.payload.size()!=kRouteReceiptPayloadBytes||
      route_wire::get32(view.payload,0)!=1)return false;
    RouteReceipt draft{};
    draft.kind=static_cast<RouteEventKind>(route_wire::get32(view.payload,4));
    draft.candidate.colony=route_wire::get32(view.payload,8);
    draft.candidate.id=route_wire::get32(view.payload,12);
    draft.candidate.from=route_wire::get32(view.payload,16);
    draft.candidate.to=route_wire::get32(view.payload,20);
    draft.candidate.kind=static_cast<RouteKind>(route_wire::get32(view.payload,24));
    const auto cost=route_wire::get32(view.payload,28);
    const auto risk=route_wire::get32(view.payload,32);
    const auto guide=route_wire::get32(view.payload,36);
    if(cost>1000||risk>1000||guide>1000)return false;
    draft.candidate.digital_cost=static_cast<std::uint16_t>(cost);
    draft.candidate.model_risk=static_cast<std::uint16_t>(risk);
    draft.candidate.guidance_weight=static_cast<std::uint16_t>(guide);
    draft.sequence=route_wire::get64(view.payload,40);
    draft.change_id=route_wire::get64(view.payload,48);
    draft.revision=route_wire::get64(view.payload,56);
    static constexpr char digits[]="0123456789abcdef";
    draft.state_sha256.resize(64);
    for(std::size_t i=0;i<32;++i) {
      const auto v=std::to_integer<std::uint8_t>(view.payload[64+i]);
      draft.state_sha256[2*i]=digits[v>>4];
      draft.state_sha256[2*i+1]=digits[v&15];
    }
    if(!route_wire::valid(draft)||view.header.sequence!=draft.sequence||
      view.header.stream_id!=draft.candidate.colony)return false;
    out=std::move(draft);
    return true;
  }
};
} // namespace anaiero::optimization
