#pragma once
// Application-level SW5 adapter. No universal Cell class, file I/O, or
// physical device action; checkpoint uses existing CEVCELLSTATE/1 wire format.
#include "anaiero/contracts/simulation_engine.hpp"
#include "anaiero/services/cell_behavior_service.hpp"
#include "anaiero/infrastructure/cell_checkpoint.hpp"
#include "christeye/living/plan_hash.hpp"
#include <algorithm>
#include <cstdint>
#include <string>
#include <utility>
namespace anaiero::application {
class Sw5SimulationAdapter final:public engine::ISimulationEngine {
  cells::Atlas atlas_{};
  cells::World baseline_{};
  cells::CellBehaviorService service_;
  std::uint32_t id_,source_;
  std::uint64_t epoch_;
  bool ready_=false;
  static void digest(engine::Snapshot& snap,const std::string& bytes) noexcept {
    const auto sha=christeye::living::plans::sha256_hex(bytes);
    std::copy(sha.begin(),sha.end(),snap.state_sha256.begin());
    snap.state_sha256[64]='\0';
  }
public:
  Sw5SimulationAdapter(std::uint32_t id,std::uint32_t source,std::uint64_t epoch,
     cells::Atlas atlas,cells::World initial,cells::ISpatialPort& spatial) noexcept
      :atlas_(std::move(atlas)),baseline_(initial),service_(atlas_,spatial),
        id_(id),source_(source),epoch_(epoch){}
  engine::Descriptor describe()const noexcept override {
    return {id_,source_,epoch_,engine::Kind::sw5,atlas_.model_digest,
      "SYNTHETIC_UNCALIBRATED",true,true};
  }
  engine::Status initialize()noexcept override {
    if(!id_||!source_||!epoch_||!atlas_.synthetic_only||
       service_.restore(baseline_)!=cells::Status::ok)
      return engine::Status::invalid;
    ready_=true;return engine::Status::ok;
  }
  engine::Status latest(engine::Snapshot& out)const noexcept override {
    if(!ready_)return engine::Status::not_initialized;
    const auto& w=service_.world();const auto m=cells::metrics(w);
    out={};out.id=id_;out.source_epoch=epoch_;out.tick=w.tick;
    out.revision=w.generation;out.participants=m.viable;
    out.connections=m.linked;out.energy=m.total_energy;
    out.learning_proxy=m.total_learning;
    digest(out,cells::files::serialize_world(w,atlas_));
    return engine::Status::ok;
  }
  engine::Status advance(const engine::StepRequest& request,
                         engine::Snapshot& result) noexcept override {
    if(!ready_)return engine::Status::not_initialized;
    if(service_.world().tick!=request.expected_tick)
      return engine::Status::tick_conflict;
    const auto& p=request.inputs;
    if(service_.step({p.support_proxy,p.stress_proxy,p.digital_signal,p.lesson})
       !=cells::Status::ok)return engine::Status::advance_failed;
    return latest(result);
  }
  engine::Status checkpoint(engine::ICheckpointWriter& writer)const noexcept override {
    if(!ready_)return engine::Status::not_initialized;
    return writer.write(cells::files::serialize_world(service_.world(),atlas_))?
      engine::Status::ok:engine::Status::io_failure;
  }
  engine::Status restore(engine::ICheckpointReader& reader) noexcept override {
    cells::World value{};
    if(cells::files::deserialize_world(reader.bytes(),atlas_,value)!=cells::Status::ok)
      return engine::Status::invalid;
    if(service_.restore(value)!=cells::Status::ok)return engine::Status::invalid;
    ready_=true;return engine::Status::ok;
  }
};
} // namespace anaiero::application
