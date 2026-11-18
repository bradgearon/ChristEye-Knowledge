#pragma once
// Compiled composition registry. Factories own adapters and host resources;
// session services depend on typed ports and capability declarations.
#include "anaiero/application/sw5_simulation_adapter.hpp"
#include "anaiero/application/sim3_simulation_adapter.hpp"
#include "anaiero/application/supervised_scientific_engine.hpp"
#include "anaiero/contracts/native_session.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/memory_checkpoint.hpp"
#include "anaiero/infrastructure/scientific_solver_configuration.hpp"
#include "anaiero/services/checkpointing_engine_service.hpp"
#include <array>
#include <filesystem>
#include <memory>
#include <string>
namespace anaiero::application {
class Sw5SessionPort final:public session::ISw5Port {
  Sw5SimulationAdapter& engine_;
  session::CheckpointingEngineService& durable_;
  cells::Atlas atlas_;
  cells::ISpatialPort& spatial_;
public:
  Sw5SessionPort(Sw5SimulationAdapter& engine,session::CheckpointingEngineService& durable,
      cells::Atlas atlas,cells::ISpatialPort& spatial):
      engine_(engine),durable_(durable),atlas_(std::move(atlas)),spatial_(spatial){}
  const cells::Atlas& atlas()const noexcept override{return atlas_;}
  cells::ISpatialPort& spatial()noexcept override{return spatial_;}
  bool world(cells::World& output)const noexcept override{
    infrastructure::MemoryCheckpoint bytes;
    return engine_.checkpoint(bytes)==engine::Status::ok&&
      cells::files::deserialize_world(bytes.bytes(),atlas_,output)==cells::Status::ok;
  }
  bool encode(const cells::World& world,std::string& output)const noexcept override{
    if(cells::validate_world_invariants(world)!=cells::Status::ok)return false;
    output=cells::files::serialize_world(world,atlas_);return true;
  }
  bool replace(std::string_view checkpoint)noexcept override{
    infrastructure::MemoryCheckpoint bytes;
    return bytes.write(checkpoint)&&engine_.restore(bytes)==engine::Status::ok&&durable_.persist_latest();
  }
};
class ISessionEngineBinding {
public:
  virtual ~ISessionEngineBinding()=default;
  virtual engine::ISimulationEngine& engine()noexcept=0;
  virtual std::unique_ptr<session::ISw5Port> make_sw5_port(session::CheckpointingEngineService&){return {};}
  virtual const cells::Atlas* atlas()const noexcept{return nullptr;}
  virtual cells::ISpatialPort* spatial()noexcept{return nullptr;}
  virtual bool world(cells::World&)const noexcept{return false;}
  virtual christeye::living::sim3::Runner* sim3_runner()noexcept{return nullptr;}
  virtual void set_extension(contracts::IServiceExtension*)noexcept{}
  // Only cancel() may be called concurrently with the owning session thread.
  virtual void cancel()noexcept{}
  virtual std::string diagnostics()const{return {};}
};
namespace session_factory_detail {
class Sw5Binding final:public ISessionEngineBinding {
  cells::Atlas atlas_;
  cells::BoundedPositionPort spatial_;
  Sw5SimulationAdapter engine_;
public:
  Sw5Binding(const session::EngineConfig& config,cells::Atlas atlas,cells::World initial):
    atlas_(std::move(atlas)),engine_(config.id,config.source,config.epoch,atlas_,initial,spatial_){}
  engine::ISimulationEngine& engine()noexcept override{return engine_;}
  std::unique_ptr<session::ISw5Port> make_sw5_port(session::CheckpointingEngineService& durable)override{
    return std::make_unique<Sw5SessionPort>(engine_,durable,atlas_,spatial_);
  }
  const cells::Atlas* atlas()const noexcept override{return &atlas_;}
  cells::ISpatialPort* spatial()noexcept override{return &spatial_;}
  void set_extension(contracts::IServiceExtension* extension)noexcept override{engine_.set_extension(extension);}
  bool world(cells::World& output)const noexcept override{
    infrastructure::MemoryCheckpoint bytes;
    return engine_.checkpoint(bytes)==engine::Status::ok&&
      cells::files::deserialize_world(bytes.bytes(),atlas_,output)==cells::Status::ok;
  }
};
class Sim3Binding final:public ISessionEngineBinding {
  christeye::living::sim3::Runner runner_;
  Sim3SimulationAdapter engine_;
public:
  explicit Sim3Binding(const session::EngineConfig& config):engine_(runner_,config.id,config.source,config.epoch){}
  bool open(const session::EngineConfig& config,const std::filesystem::path& root){
    return runner_.open(config.model_path,root)==christeye::living::sim3::Result::ok;
  }
  engine::ISimulationEngine& engine()noexcept override{return engine_;}
  christeye::living::sim3::Runner* sim3_runner()noexcept override{return &runner_;}
  void set_extension(contracts::IServiceExtension* extension)noexcept override{runner_.set_extension(extension);}
};
class ScientificBinding final:public ISessionEngineBinding {
  SupervisedScientificEngine engine_;
public:
  ScientificBinding(const session::EngineConfig& config,science::ScientificConfiguration solver):
    engine_(config.id,config.source,config.epoch,std::move(solver)){}
  engine::ISimulationEngine& engine()noexcept override{return engine_;}
  void cancel()noexcept override{engine_.cancel();}
  void set_extension(contracts::IServiceExtension* extension)noexcept override{engine_.set_extension(extension);}
  std::string diagnostics()const override{return engine_.diagnostics();}
};
using Binding=std::unique_ptr<ISessionEngineBinding>;
inline Binding sw5(const session::EngineConfig& config,const std::filesystem::path&,std::string& error){
  cells::Atlas atlas{};cells::World initial{};
  if(cells::files::load_checked_atlas(config.model_path,atlas)!=cells::Status::ok||
     cells::files::read_world(config.initial_path,atlas,initial)!=cells::Status::ok){
    error="invalid SW5 model or initial state";return {};
  }
  return std::make_unique<Sw5Binding>(config,std::move(atlas),initial);
}
inline Binding sim3(const session::EngineConfig& config,const std::filesystem::path& root,std::string& error){
  auto binding=std::make_unique<Sim3Binding>(config);
  if(!binding->open(config,root)){error="invalid or locked SIM3 engine state";return {};}
  return binding;
}
inline Binding scientific(const session::EngineConfig& config,const std::filesystem::path&,std::string& error){
  science::ScientificConfiguration solver;
  if(!science::files::read_solver_configuration(config.model_path,solver,error))return {};
  if(!config.solver_configuration_sha256.empty()&&config.solver_configuration_sha256!=solver.configuration_sha256){
    error="scientific configuration changed before engine construction";return {};
  }
  if(!solver.expected.checkpoint_schema){error="durable scientific sessions require provider checkpoints";return {};}
  if(config.inputs.support_proxy||config.inputs.stress_proxy||config.inputs.digital_signal||config.inputs.lesson){
    error="scientific input belongs in its solver configuration";return {};
  }
  return std::make_unique<ScientificBinding>(config,std::move(solver));
}
} // namespace session_factory_detail
class SessionEngineFactoryRegistry final {
  using Factory=session_factory_detail::Binding(*)(const session::EngineConfig&,const std::filesystem::path&,std::string&);
  struct Entry {engine::Kind kind;Factory create;};
  static constexpr std::array<Entry,3> entries_{{
    {engine::Kind::sw5,session_factory_detail::sw5},
    {engine::Kind::sim3,session_factory_detail::sim3},
    {engine::Kind::scientific,session_factory_detail::scientific}
  }};
public:
  static std::unique_ptr<ISessionEngineBinding> create(const session::EngineConfig& config,
      const std::filesystem::path& engine_root,std::string& error){
    error.clear();
    if(!config.id||!config.source||!config.epoch){error="invalid engine identity";return {};}
    for(const auto& entry:entries_)if(entry.kind==config.kind)return entry.create(config,engine_root,error);
    error="engine kind is not compiled into this host";return {};
  }
};
} // namespace anaiero::application
