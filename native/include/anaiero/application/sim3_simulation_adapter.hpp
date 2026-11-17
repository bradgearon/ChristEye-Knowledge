#pragma once
// Existing durable SIM3 Runner adapter. Its storage is already stateful and
// controls its own committed checkpoints. Arbitrary rewinding is unsupported.
#include "anaiero/contracts/simulation_engine.hpp"
#include "christeye/living/colony_runner.hpp"
#include <algorithm>
#include <string>
namespace anaiero::application {
class Sim3SimulationAdapter final:public engine::ISimulationEngine {
  christeye::living::sim3::Runner& runner_;
  std::uint32_t id_,source_;
  std::uint64_t epoch_;
public:
  Sim3SimulationAdapter(christeye::living::sim3::Runner& runner,
       std::uint32_t id,std::uint32_t source,std::uint64_t epoch)noexcept:
    runner_(runner),id_(id),source_(source),epoch_(epoch){}
  engine::Descriptor describe()const noexcept override {
    return {id_,source_,epoch_,engine::Kind::sim3,
      runner_.configuration().hash,"SYNTHETIC_UNCALIBRATED",true,false};
  }
  engine::Status initialize()noexcept override {
    return runner_.is_open()&&id_&&source_&&epoch_?
      engine::Status::ok:engine::Status::not_initialized;
  }
  engine::Status latest(engine::Snapshot& result)const noexcept override {
    if(!runner_.is_open())return engine::Status::not_initialized;
    const auto& state=runner_.state();
    result={};
    result.id=id_;result.source_epoch=epoch_;result.tick=state.tick;
    result.revision=state.tick;
    // Dead lineage records remain in the durable model, but do not count as viable.
    result.participants=static_cast<std::uint32_t>(std::count_if(state.cells.begin(),state.cells.end(),
      [](const auto& cell){return cell.alive;}));
    result.connections=static_cast<std::uint32_t>(runner_.configuration().links.size());
    for(const auto& cell:state.cells) {
      result.energy+=cell.energy;result.learning_proxy+=cell.learning;
    }
    const auto sha=runner_.snapshot_sha256();
    std::copy(sha.begin(),sha.end(),result.state_sha256.begin());
    result.state_sha256[64]='\0';
    return engine::Status::ok;
  }
  engine::Status advance(const engine::StepRequest& step,
                         engine::Snapshot& result) noexcept override {
    if(!runner_.is_open())return engine::Status::not_initialized;
    if(step.expected_tick!=runner_.state().tick)return engine::Status::tick_conflict;
    // SIM3 has its own synthetic input/plans. Never silently translate an
    // unrelated SW5 signal into a SIM3 physical or modeling instruction.
    const auto& in=step.inputs;
    if(in.support_proxy||in.stress_proxy||in.digital_signal||in.lesson)
      return engine::Status::unsupported;
    if(runner_.step(false)!=christeye::living::sim3::Result::ok)
      return engine::Status::io_failure;
    return latest(result);
  }
  engine::Status checkpoint(engine::ICheckpointWriter& writer)const noexcept override {
    if(!runner_.is_open()||!runner_.audit_disk())return engine::Status::io_failure;
    return writer.write(christeye::living::sim3::encode_snapshot(runner_.state()))?
      engine::Status::ok:engine::Status::io_failure;
  }
  engine::Status restore(engine::ICheckpointReader&)noexcept override {
    // Reopen existing Runner from its committed culture.state in a fresh
    // process instead. Never bypass its file lock / CAS history invariant.
    return engine::Status::unsupported;
  }
};
} // namespace anaiero::application
