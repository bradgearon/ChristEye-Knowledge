#pragma once
// Bounded, committed WindStore event stream for epoch-scoped observation frames.
// Validates every byte on replay; rejects conflicting IDs across restarts.
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/infrastructure/epoch_observation_codec.hpp"
#include "christeye/living/durable_registry.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
namespace anaiero::runtime {
class WindStoreEpochObservationJournal final: public IObservationEpochJournal {
  christeye::living::durable::Registry& store_;
  std::string session_;
  EpochObservationCodec codec_{};
  struct Cursor {std::uint32_t source=0;std::uint64_t epoch=0,last_sequence=0;};
public:
  WindStoreEpochObservationJournal(christeye::living::durable::Registry& store,
                                   std::string session):store_(store),session_(std::move(session)){}
  bool commit(const Observation& o,std::span<const std::byte> wire) noexcept override {
    using namespace christeye::living::durable;
    if(!store_.is_open()||!valid_id(session_)||wire.size()!=kEpochObservationFrame)
      return false;
    Observation decoded{};
    if(!codec_.decode(wire,decoded)||decoded.source!=o.source||
       decoded.source_epoch!=o.source_epoch||decoded.sequence!=o.sequence||
       decoded.metric!=o.metric||decoded.colony!=o.colony||decoded.animon!=o.animon||
       decoded.value!=o.value||decoded.evidence_tag!=o.evidence_tag||
       decoded.acquired_ns!=o.acquired_ns||decoded.origin!=o.origin||
       decoded.lane!=o.lane||decoded.clock_domain!=o.clock_domain||
       decoded.urgency!=o.urgency||decoded.importance!=o.importance)return false;
    const std::string_view bytes(reinterpret_cast<const char*>(wire.data()),wire.size());
    const auto digest=christeye::living::plans::sha256_hex(bytes);
    const auto clock="epoch-"+std::to_string(o.source_epoch);
    const auto producer="source-"+std::to_string(o.source);
    for(const auto& event:store_.events())
      if(event.kind=="OBSERVATION_RECORDED"&&event.entity==session_&&
         event.clock==clock&&event.producer==producer&&event.source_tick==o.sequence)
        return event.object_hash==digest;
    const auto data=store_.put(bytes);
    if(data.code!=Code::ok||data.hash!=digest)return false;
    const auto published=store_.publish("OBSERVATION_RECORDED",session_,digest,
       o.origin==Origin::simulated?"SIMULATED":"IMPORTED_UNVERIFIED",
       o.sequence,clock,producer);
    return published.code==Code::ok;
  }
  bool replay(IObservationEpochReplay& reader) noexcept override {
    if(!store_.is_open()||!christeye::living::durable::valid_id(session_))return false;
    std::array<Cursor,kMaxSources> cursors{};
    std::size_t used=0;
    for(const auto& event:store_.events()){
      if(event.kind!="OBSERVATION_RECORDED"||event.entity!=session_)continue;
      std::string raw;
      if(!store_.get(event.object_hash,raw)||raw.size()!=kEpochObservationFrame)return false;
      const auto frame=std::span<const std::byte>(
        reinterpret_cast<const std::byte*>(raw.data()),raw.size());
      Observation decoded{};
      if(!codec_.decode(frame,decoded)||
         event.source_tick!=decoded.sequence||
         event.clock!="epoch-"+std::to_string(decoded.source_epoch)||
         event.producer!="source-"+std::to_string(decoded.source)||
         event.origin!=(decoded.origin==Origin::simulated?"SIMULATED":"IMPORTED_UNVERIFIED"))
        return false;
      std::size_t index=0;
      for(;index<used;++index)
        if(cursors[index].source==decoded.source&&
           cursors[index].epoch==decoded.source_epoch)break;
      if(index==used){
        if(used==kMaxSources)return false;
        cursors[used++]={decoded.source,decoded.source_epoch,0};
      }
      if(decoded.sequence!=cursors[index].last_sequence+1)return false;
      if(!reader.accept(frame))return false;
      cursors[index].last_sequence=decoded.sequence;
    }
    return true;
  }
};
} // namespace anaiero::runtime
