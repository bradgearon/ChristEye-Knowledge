#pragma once
// Durable local WindStore route receipt journal. Not remote authentication.
// Canonical WindPacket bytes are the immutable CAS object.
#include "anaiero/contracts/route_receipt_delivery.hpp"
#include "christeye/living/durable_registry.hpp"
#include "christeye/living/plan_hash.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace anaiero::optimization {
class WindStoreRouteReceiptJournal final:public IRouteReceiptJournal {
  christeye::living::durable::Registry& registry_;
  std::string session_;
  static constexpr std::string_view kKind="DIGITAL_ROUTE_RECEIPT";
  static constexpr std::string_view kProducer="anaiero-route";
public:
  WindStoreRouteReceiptJournal(christeye::living::durable::Registry& registry,
    std::string session):registry_(registry),session_(std::move(session)){}
  bool append(const RouteReceipt& receipt,std::span<const std::byte> frame) noexcept override {
    using namespace christeye::living::durable;
    if(!registry_.is_open()||!valid_id(session_)||
       frame.size()!=kRouteReceiptPacketBytes||!receipt.sequence)return false;
    const std::string_view bytes(reinterpret_cast<const char*>(frame.data()),frame.size());
    const auto hash=christeye::living::plans::sha256_hex(bytes);
    // A session/sequence is immutable even if somebody sends a different
    // valid frame using the same sequence: reject rather than record a fork.
    Event existing;bool found=false;
    if(!registry_.find_event(kKind,session_,"simulation",kProducer,receipt.sequence,existing,found))return false;
    if(found)return existing.object_hash==hash;
    const auto result=registry_.put(bytes);
    if(result.code!=Code::ok||result.hash!=hash)return false;
    const auto published=registry_.publish(std::string(kKind),session_,hash,
      "SIMULATED",receipt.sequence,"simulation",std::string(kProducer));
    return published.code==Code::ok;
  }
  bool replay(IRouteReceiptReplay& visitor) noexcept override {
    if(!registry_.is_open()||
       !christeye::living::durable::valid_id(session_))return false;
    std::uint64_t sequence=0;
    return registry_.visit_events(0,[&](const christeye::living::durable::Event& event){
      if(event.kind!=kKind||event.entity!=session_)return true;
      if(event.producer!=kProducer||event.origin!="SIMULATED"||
         event.clock!="simulation"||event.source_tick!=sequence+1)return false;
      std::string data;
      if(!registry_.get(event.object_hash,data)||data.size()!=kRouteReceiptPacketBytes)
        return false;
      const auto bytes=std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(data.data()),data.size());
      if(!visitor.on_frame(bytes))return false;
      ++sequence;return true;
    });
  }
};
} // namespace anaiero::optimization
