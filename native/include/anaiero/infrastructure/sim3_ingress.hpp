#pragma once
// Source adapter for SIM3 snapshots: every movement crosses the typed bus.
#include "anaiero/infrastructure/windbus_adapter.hpp"
#include "christeye/living/colony_runner.hpp"
#include <array>
#include <algorithm>
#include <cstdint>
namespace anaiero::runtime {
class Sim3Ingress {
  IObservationRuntime& runtime_;
  std::array<std::uint64_t,8> sequences_{};
  Status send(std::uint32_t colony,std::uint32_t animon,Metric metric,
              std::int32_t value,std::uint64_t ns,std::uint64_t tag,
              std::uint16_t urgency=100) noexcept {
    if(colony==0||colony>sequences_.size())return Status::invalid;
    Observation o{};
    o.colony=colony;o.animon=animon;o.source=colony;o.clock_domain=1;
    o.metric=metric;o.value=value;o.sequence=++sequences_[colony-1];
    o.acquired_ns=ns;o.evidence_tag=tag;o.origin=Origin::simulated;
    o.lane=metric==Metric::digital_desire?Lane::desire:
           health_metric(metric)?Lane::health:Lane::telemetry;
    o.urgency=urgency;o.importance=200;
    // The adapter must serialize via the exact native WindPacket/1 transport.
    std::array<std::byte,kObservationFrame> frame{};
    std::size_t count=0;
    if(!encode_observation(o,frame,count))return Status::malformed_wire;
    // Drain in bounded batches, avoiding an unbounded observation backlog.
    if(runtime_.pending()>=kMaxWork/2)runtime_.drain(kMaxWork/2);
    return ingress_frame(runtime_,std::span<const std::byte>(frame.data(),count));
  }
public:
  explicit Sim3Ingress(IObservationRuntime& service) noexcept : runtime_(service) {}
  Status enroll(const christeye::living::sim3::Configuration& cfg) noexcept {
    if(cfg.colonies.size()>sequences_.size())return Status::invalid;
    for(std::size_t i=0;i<cfg.colonies.size();++i){
      if(runtime_.enroll({static_cast<std::uint32_t>(i+1),1,Origin::simulated,true,true})!=Status::ok)
        return Status::invalid;
    }
    return Status::ok;
  }
  // The input is a snapshot from an already committed simulation tick.
  // The digital desire is a synthetic software preference for learning work;
  // it is never a measured physiological need or a care plan.
  Status import(const christeye::living::sim3::Configuration& cfg,
                const christeye::living::sim3::Snapshot& snapshot) noexcept {
    using namespace christeye::living;
    std::uint64_t tag=0;
    const auto hash=christeye::living::plans::sha256_hex(sim3::encode_snapshot(snapshot));
    for(unsigned i=0;i<16;++i){const char c=hash[i];const unsigned digit=c<='9'?unsigned(c-'0'):unsigned(c-'a'+10);tag=(tag<<4)|digit;}
    const auto ns=snapshot.tick*1000000ULL;
    for(std::size_t i=0;i<cfg.colonies.size();++i){
      const auto& name=cfg.colonies[i].id;
      const auto colony=static_cast<std::uint32_t>(i+1);
      for(const auto& c:snapshot.colonies){
        if(c.id!=name)continue;
        if(const auto s=send(colony,0,Metric::support,c.food,ns,tag);s!=Status::ok)return s;
        if(const auto s=send(colony,0,Metric::stress,c.stress,ns,tag);s!=Status::ok)return s;
        if(const auto s=send(colony,0,Metric::activity,c.signal,ns,tag);s!=Status::ok)return s;
      }
      for(const auto& c:snapshot.cells){
        if(c.colony!=name)continue;
        if(const auto s=send(colony,c.id,Metric::viability,c.alive?1:0,ns,tag);s!=Status::ok)return s;
        if(const auto s=send(colony,c.id,Metric::energy,c.energy,ns,tag);s!=Status::ok)return s;
        if(const auto s=send(colony,c.id,Metric::digital_learning,c.learning,ns,tag);s!=Status::ok)return s;
        // A deterministic digital preference; no biological inference.
        const auto desire=std::clamp(100-c.learning,0,100);
        if(const auto s=send(colony,c.id,Metric::digital_desire,desire,ns,tag,static_cast<std::uint16_t>(desire*8));s!=Status::ok)return s;
      }
    }
    return Status::ok;
  }
};
} // namespace anaiero::runtime
