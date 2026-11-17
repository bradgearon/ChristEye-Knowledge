#pragma once
// Composition only: one session owner, one separate observation-store owner.
#include "anaiero/application/sw5_simulation_adapter.hpp"
#include "anaiero/application/sim3_simulation_adapter.hpp"
#include "anaiero/infrastructure/session_files.hpp"
#include "anaiero/infrastructure/session_wakeup.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/memory_checkpoint.hpp"
#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/infrastructure/async_epoch_journal_worker.hpp"
#include "anaiero/infrastructure/windstore_epoch_observation_journal.hpp"
#include "anaiero/infrastructure/epoch_runtime_projection.hpp"
#include "anaiero/services/runtime_service.hpp"
#include "anaiero/services/epoch_observation_delivery_service.hpp"
#include "anaiero/services/checkpointing_engine_service.hpp"
#include "anaiero/services/native_session_service.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
namespace anaiero::application {
class Sw5SessionPort final:public session::ISw5Port {
  Sw5SimulationAdapter& engine_;
  session::CheckpointingEngineService& durable_;
  cells::Atlas atlas_;
  cells::ISpatialPort& spatial_;
public:
  Sw5SessionPort(Sw5SimulationAdapter& engine,session::CheckpointingEngineService& durable,
    cells::Atlas atlas,cells::ISpatialPort& spatial):engine_(engine),durable_(durable),atlas_(std::move(atlas)),spatial_(spatial){}
  const cells::Atlas& atlas()const noexcept override{return atlas_;}
  cells::ISpatialPort& spatial()noexcept override{return spatial_;}
  bool world(cells::World& out)const noexcept override{
    infrastructure::MemoryCheckpoint data;
    return engine_.checkpoint(data)==engine::Status::ok&&
      cells::files::deserialize_world(data.bytes(),atlas_,out)==cells::Status::ok;
  }
  bool encode(const cells::World& world,std::string& out)const noexcept override{
    if(cells::validate_world_invariants(world)!=cells::Status::ok)return false;
    out=cells::files::serialize_world(world,atlas_);return true;
  }
  bool replace(std::string_view checkpoint)noexcept override{
    infrastructure::MemoryCheckpoint bytes;
    return bytes.write(checkpoint)&&engine_.restore(bytes)==engine::Status::ok&&durable_.persist_latest();
  }
};
class NativeSessionApplication final {
  using Registry=christeye::living::durable::Registry;
  struct EngineOwner {
    std::unique_ptr<christeye::living::sim3::Runner> runner;
    std::unique_ptr<engine::ISimulationEngine> engine;
    std::unique_ptr<session::CheckpointingEngineService> durable;
    std::unique_ptr<Sw5SessionPort> sw5;
  };
  session::Configuration config_{};
  Registry metadata_,observations_;
  runtime::WeightedQueue<> queue_;
  runtime::RuntimeService runtime_{queue_};
  runtime::EpochRuntimeProjectionPort projection_{runtime_};
  cells::BoundedPositionPort spatial_;
  engine::SimulationCoordinatorService coordinator_;
  engine::WindStoreCheckpointCatalog catalog_{metadata_};
  session::files::CatalogCheckpointStore checkpoints_{catalog_};
  std::unique_ptr<session::files::WindStoreRecordStore> records_;
  std::array<EngineOwner,engine::kMaxEngines> engines_{};
  runtime::EpochObservationCodec codec_;
  std::unique_ptr<runtime::WindStoreEpochObservationJournal> journal_;
  session::SessionWakeup wakeup_;
  std::unique_ptr<runtime::AsyncEpochJournalWorker<32>> worker_;
  std::unique_ptr<runtime::AsyncObservationCommitService> evidence_;
  class Budget final:public session::IWorkBudget {
    Registry& metadata_;runtime::AsyncEpochJournalWorker<32>& worker_;
    std::size_t initial_observations_;
  public:
    Budget(Registry& metadata,runtime::AsyncEpochJournalWorker<32>& worker,std::size_t initial):
      metadata_(metadata),worker_(worker),initial_observations_(initial){}
    bool admits(std::size_t engines)const noexcept override{
      constexpr auto cap=christeye::living::durable::kMaxEvents;
      const auto accepted=worker_.status().accepted;
      const auto reserve=32+engines*16;
      return metadata_.event_count()+reserve<cap&&
        initial_observations_+accepted+engines*5+16<cap;
    }
  };
  std::unique_ptr<Budget> budget_;
  std::unique_ptr<session::NativeSessionService> service_;
  std::string error_;
public:
  explicit NativeSessionApplication(runtime::IAsyncCompletionSignal* observer=nullptr):wakeup_(observer){}
  NativeSessionApplication(const NativeSessionApplication&)=delete;
  NativeSessionApplication& operator=(const NativeSessionApplication&)=delete;
  bool open(const std::filesystem::path& configuration,const std::filesystem::path& root){
    namespace db=christeye::living::durable;
    if(service_)return false;
    auto fail=[&](std::string message){error_=std::move(message);return false;};
    if(!session::files::read_configuration(configuration,config_,error_))return false;
    if(metadata_.open(root/"session")!=db::Code::ok||observations_.open(root/"evidence")!=db::Code::ok)
      return fail("session storage is unavailable, locked, or corrupt");
    records_=std::make_unique<session::files::WindStoreRecordStore>(metadata_,"session-"+config_.name);
    auto saved=std::make_unique<session::Record>();bool exists=false;
    if(!records_->load(*saved,exists))return fail("invalid session recovery manifest");
    if(!exists){
      for(const auto& event:metadata_.events())
        if(event.producer=="anaiero-session")return fail("state directory belongs to a different session");
      if(observations_.event_count()!=0)return fail("evidence exists without its session manifest");
    }
    if(exists&&(saved->configuration_sha256!=config_.digest||saved->coordinator.engine_count!=config_.count))
      return fail("session configuration/model identity changed");
    std::array<engine::ISimulationEngine*,engine::kMaxEngines> generic{};
    std::array<session::ISw5Port*,engine::kMaxEngines> sw5{};
    for(std::size_t i=0;i<config_.count;++i){
      const auto& cfg=config_.engines[i];auto& owner=engines_[i];
      cells::Atlas atlas{};Sw5SimulationAdapter* sw5_adapter=nullptr;
      if(cfg.kind==engine::Kind::sw5){
        cells::World initial{};
        if(cells::files::load_checked_atlas(cfg.model_path,atlas)!=cells::Status::ok||
           cells::files::read_world(cfg.initial_path,atlas,initial)!=cells::Status::ok)
          return fail("invalid SW5 model or initial state");
        auto adapter=std::make_unique<Sw5SimulationAdapter>(cfg.id,cfg.source,cfg.epoch,atlas,initial,spatial_);
        sw5_adapter=adapter.get();owner.engine=std::move(adapter);
      }else{
        owner.runner=std::make_unique<christeye::living::sim3::Runner>();
        if(owner.runner->open(cfg.model_path,root/("engine-"+std::to_string(cfg.id)))!=christeye::living::sim3::Result::ok)
          return fail("invalid or locked SIM3 engine state");
        owner.engine=std::make_unique<Sim3SimulationAdapter>(*owner.runner,cfg.id,cfg.source,cfg.epoch);
      }
      if(owner.engine->initialize()!=engine::Status::ok)return fail("engine initialize failed");
      if(exists){
        engine::CheckpointRecord checkpoint{};std::string bytes;
        if(!catalog_.latest(cfg.id,cfg.epoch,checkpoint,bytes)||checkpoint.source!=cfg.source||
           checkpoint.model_sha256!=owner.engine->describe().model_digest)return fail("checkpoint identity mismatch");
        if(cfg.kind==engine::Kind::sw5){
          infrastructure::MemoryCheckpoint data;
          if(!data.write(bytes)||owner.engine->restore(data)!=engine::Status::ok)return fail("SW5 checkpoint restore failed");
        }
      }
      owner.durable=std::make_unique<session::CheckpointingEngineService>(*owner.engine,checkpoints_);
      if(!exists&&!owner.durable->persist_latest())return fail("initial engine checkpoint failed");
      if(exists&&saved->coordinator.cycle_active&&!saved->coordinator.publication_pending&&
         saved->coordinator.next_slot==i){
        const auto& pending=saved->coordinator.engines[i];
        owner.durable->authorize_replay({pending.last.tick,pending.cycle_inputs,saved->coordinator.completed_cycles+1});
      }
      if(!coordinator_.register_engine(*owner.durable,cfg.cadence))return fail("engine registration failed");
      generic[i]=owner.durable.get();
      if(sw5_adapter){
        owner.sw5=std::make_unique<Sw5SessionPort>(*sw5_adapter,*owner.durable,atlas,spatial_);
        sw5[i]=owner.sw5.get();
      }
      if(!projection_.enroll({cfg.source,1,runtime::Origin::simulated,true,false,false,0}))
        return fail("source enrollment failed");
    }
    journal_=std::make_unique<runtime::WindStoreEpochObservationJournal>(observations_,config_.name);
    runtime::EpochObservationDeliveryService replay(codec_,*journal_,projection_);
    if(!replay.recover())return fail("evidence replay failed");
    worker_=std::make_unique<runtime::AsyncEpochJournalWorker<32>>(*journal_,codec_,&wakeup_);
    budget_=std::make_unique<Budget>(metadata_,*worker_,observations_.event_count());
    evidence_=std::make_unique<runtime::AsyncObservationCommitService>(*worker_,projection_);
    service_=std::make_unique<session::NativeSessionService>(config_,coordinator_,*records_,*evidence_,projection_,generic,sw5);
    service_->set_budget(budget_.get());
    if(!service_->start(std::move(*saved),exists))return fail("session continuation failed: "+service_->state_json());
    return true;
  }
  session::NativeSessionService& service()noexcept{return *service_;}
  session::SessionWakeup& wakeup()noexcept{return wakeup_;}
  const session::Configuration& configuration()const noexcept{return config_;}
  const std::string& error()const noexcept{return error_;}
  bool settle(std::chrono::milliseconds timeout=std::chrono::seconds(30)){
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    while(service_->busy()&&!service_->failed()){
      const auto observed=wakeup_.generation();
      if(!service_->pump()){
        if(std::chrono::steady_clock::now()>=deadline)return false;
        wakeup_.wait(observed,deadline);
      }
    }
    return !service_->failed();
  }
  std::size_t committed_observations_after_shutdown(){worker_->shutdown();return observations_.event_count();}
};
} // namespace anaiero::application
