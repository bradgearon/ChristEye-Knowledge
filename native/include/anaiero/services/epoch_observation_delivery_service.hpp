#pragma once
// Journal first, then project and ACK; restart replays every configured
// observation source and source epoch. Single-writer, no silent gap repair.
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/infrastructure/epoch_observation_codec.hpp"
namespace anaiero::runtime {
class EpochObservationDeliveryService final:private IObservationEpochReplay {
  IObservationEpochJournal& journal_;
  IObservationEpochProjection& projection_;
  EpochObservationCodec codec_{};
  bool ready_=false;
  bool accept(std::span<const std::byte> wire) noexcept override {
    Observation decoded{};
    return codec_.decode(wire,decoded)&&projection_.apply(decoded);
  }
public:
  EpochObservationDeliveryService(IObservationEpochJournal& journal,
    IObservationEpochProjection& projection) noexcept:
    journal_(journal),projection_(projection){}
  bool recover() noexcept {
    if(ready_||!journal_.replay(*this))return false;
    ready_=true;return true;
  }
  bool submit(const Observation& observation) noexcept {
    if(!ready_||observation.source_epoch==0)return false;
    std::array<std::byte,kEpochObservationFrame> frame{};
    return projection_.can_accept(observation)&&codec_.encode(observation,frame)&&
      journal_.commit(observation,frame)&&
      projection_.apply(observation);
  }
};
} // namespace anaiero::runtime
