#pragma once
// Journal-owner adapter. Global cursor checkpoints and source/epoch cursors
// permit bounded snapshot+tail recovery while immutable archives retain evidence.
#include "anaiero/contracts/epoch_observation.hpp"
#include "anaiero/infrastructure/epoch_observation_codec.hpp"
#include "anaiero/infrastructure/observation_checkpoint_codec.hpp"
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
  IObservationSnapshotBuilder* snapshot_=nullptr;
  EpochObservationCodec codec_{};
  struct Cursor {std::uint32_t source=0;std::uint64_t epoch=0,last_sequence=0;};
  bool checkpoint_boundary() {
    if(!snapshot_||!store_.is_segmented())return true;
    const auto capacity=store_.capacity();
    if(!capacity.total_events||capacity.total_events%capacity.event_limit)return true;
    return checkpoint_now();
  }
public:
  WindStoreEpochObservationJournal(christeye::living::durable::Registry& store,
    std::string session,IObservationSnapshotBuilder* snapshot=nullptr):
    store_(store),session_(std::move(session)),snapshot_(snapshot){}
  // Invoke only on the Registry owner, including capacity/reservation access.
  auto capacity()const noexcept{return store_.capacity();}
  bool can_append(std::size_t count,std::size_t bytes)const noexcept{return store_.can_append(count,bytes);}
  ObservationJournalCapacity storage_capacity()const noexcept override {
    const auto c=store_.capacity();ObservationJournalCapacity result;
    result.known=true;result.sustained=c.segmented;result.writable=store_.is_open();
    result.available_events=c.segmented?UINT64_MAX-c.total_events:c.event_limit-c.resident_events;
    result.available_events=result.available_events>=c.reserved_events?result.available_events-c.reserved_events:0;
    result.available_bytes=c.available_disk_bytes>=c.minimum_free_bytes?c.available_disk_bytes-c.minimum_free_bytes:0;
    result.available_bytes=result.available_bytes>=c.reserved_bytes?result.available_bytes-c.reserved_bytes:0;
    if(!c.segmented)result.available_bytes=std::min(result.available_bytes,static_cast<std::uint64_t>(c.byte_limit)-c.tail_bytes);
    result.append_reservation_bytes=c.segmented?std::max<std::uint64_t>(512*1024,c.byte_limit+32768):1024;
    result.total_events=c.total_events;result.resident_events=c.resident_events;result.archived_segments=c.archived_segments;
    return result;
  }
  bool checkpoint_now() {
    if(!snapshot_||!store_.is_segmented())return false;
    std::string bytes;
    return ObservationCheckpointCodec::encode(snapshot_->snapshot(),bytes,session_)&&
      store_.save_replay_checkpoint(bytes)&&store_.archive_ready()==christeye::living::durable::Code::ok;
  }
  bool commit(const Observation& o,std::span<const std::byte> wire) noexcept override {
    using namespace christeye::living::durable;
    if(!store_.is_open()||!valid_id(session_)||wire.size()!=kEpochObservationFrame)return false;
    std::array<std::byte,kEpochObservationFrame> canonical{};
    if(!codec_.encode(o,canonical)||!std::equal(wire.begin(),wire.end(),canonical.begin()))return false;
    const std::string_view bytes(reinterpret_cast<const char*>(wire.data()),wire.size());
    const auto digest=christeye::living::plans::sha256_hex(bytes);
    const auto clock="epoch-"+std::to_string(o.source_epoch);
    const auto producer="source-"+std::to_string(o.source);
    Event existing;bool found=false;
    if(!store_.find_event("OBSERVATION_RECORDED",session_,clock,producer,o.sequence,existing,found))return false;
    if(found)return existing.object_hash==digest&&checkpoint_boundary();
    if(snapshot_&&!snapshot_->can_accept(o))return false;
    const auto data=store_.put(bytes);
    if(data.code!=Code::ok||data.hash!=digest)return false;
    const auto published=store_.publish("OBSERVATION_RECORDED",session_,digest,
       o.origin==Origin::simulated?"SIMULATED":"IMPORTED_UNVERIFIED",o.sequence,clock,producer);
    if(published.code!=Code::ok|| (snapshot_&&!snapshot_->apply(o)))return false;
    return checkpoint_boundary();
  }
  bool replay(IObservationEpochReplay& reader) noexcept override {
    using namespace christeye::living::durable;
    if(!store_.is_open()||!valid_id(session_))return false;
    std::array<Cursor,kMaxSources> cursors{};std::size_t used=0;std::uint64_t after=0;
    ReplayCheckpoint checkpoint;std::string bytes;bool found=false;
    if(!store_.load_replay_checkpoint(checkpoint,bytes,found))return false;
    if(found&&reader.supports_checkpoint()){
      ObservationReplayCheckpoint saved;
      if(!ObservationCheckpointCodec::decode(bytes,saved,session_)||!reader.restore(saved)||
         (snapshot_&&!snapshot_->restore(saved)))return false;
      after=checkpoint.sequence;used=saved.count;
      for(std::size_t i=0;i<used;++i)cursors[i]={saved.last[i].source,saved.last[i].source_epoch,saved.last[i].sequence};
    }
    return store_.visit_events(after,[&](const Event& event){
      if(event.kind!="OBSERVATION_RECORDED"||event.entity!=session_)return true;
      std::string raw;
      if(!store_.get(event.object_hash,raw)||raw.size()!=kEpochObservationFrame)return false;
      const auto frame=std::span<const std::byte>(reinterpret_cast<const std::byte*>(raw.data()),raw.size());
      Observation decoded{};
      if(!codec_.decode(frame,decoded)||event.source_tick!=decoded.sequence||
         event.clock!="epoch-"+std::to_string(decoded.source_epoch)||event.producer!="source-"+std::to_string(decoded.source)||
         event.origin!=(decoded.origin==Origin::simulated?"SIMULATED":"IMPORTED_UNVERIFIED"))return false;
      std::size_t index=0;
      for(;index<used;++index)if(cursors[index].source==decoded.source&&cursors[index].epoch==decoded.source_epoch)break;
      if(index==used){if(used==kMaxSources)return false;cursors[used++]={decoded.source,decoded.source_epoch,0};}
      if(cursors[index].last_sequence==UINT64_MAX||decoded.sequence!=cursors[index].last_sequence+1||
         (snapshot_&&!snapshot_->can_accept(decoded))||!reader.accept(frame)||(snapshot_&&!snapshot_->apply(decoded)))return false;
      cursors[index].last_sequence=decoded.sequence;return true;
    });
  }
};
} // namespace anaiero::runtime
