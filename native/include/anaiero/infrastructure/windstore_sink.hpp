#pragma once
// Optional host-only durable evidence adapter. Not for hard-realtime IRQ paths.
#include "anaiero/infrastructure/windbus_adapter.hpp"
#include "christeye/living/durable_registry.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace anaiero::runtime {
class WindStoreSink final : public IEventCommit {
  christeye::living::durable::Registry& store_;
public:
  explicit WindStoreSink(christeye::living::durable::Registry& store) noexcept : store_(store) {}
  bool commit(const Observation& update) noexcept override {
    if(update.origin!=Origin::simulated)return false; // read-only external enrollment unfinished
    std::array<std::byte,kObservationFrame> frame{};std::size_t length=0;
    if(!encode_observation(update,frame,length))return false;
    const auto bytes=std::string_view(reinterpret_cast<const char*>(frame.data()),length);
    const auto obj=store_.put(bytes);
    if(obj.code!=christeye::living::durable::Code::ok)return false;
    const auto receipt=store_.publish("CULTURE_CHECKPOINT",
      "colony-"+std::to_string(update.colony),obj.hash,"SIMULATED",
      update.sequence,"synthetic-tick","anaiero-projector");
    return receipt.code==christeye::living::durable::Code::ok;
  }
};
} // namespace anaiero::runtime
