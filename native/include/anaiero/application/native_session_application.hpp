#pragma once
// Composition only: one session owner, one separate observation-store owner.
#include "anaiero/application/session_engine_factories.hpp"
#include "anaiero/application/session_experiment_runtime.hpp"
#include "anaiero/infrastructure/session_files.hpp"
#include "anaiero/infrastructure/session_wakeup.hpp"
#include "anaiero/infrastructure/session_view_encoder.hpp"
#include "anaiero/infrastructure/operation_store.hpp"
#include "anaiero/infrastructure/service_trace.hpp"
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
#include "anaiero/services/operation_service.hpp"
#include "anaiero/services/observation_checkpoint_service.hpp"
#include <chrono>
#include <filesystem>
#include <memory>
namespace anaiero::application {
class NativeSessionApplication final:public session::IEngineCancellation,private contracts::IServiceExtension {
  using Registry=christeye::living::durable::Registry;
  struct EngineOwner {
    std::unique_ptr<ISessionEngineBinding> binding;
    std::unique_ptr<session::CheckpointingEngineService> durable;
    std::unique_ptr<session::ISw5Port> sw5;
  };
  session::Configuration config_{};
  session::JsonSessionViewEncoder view_encoder_;
  infrastructure::ServiceTrace trace_;
  contracts::IServiceExtension* extension_=nullptr;
  Registry metadata_,observations_;
  runtime::WeightedQueue<> queue_;
  runtime::RuntimeService runtime_{queue_};
  runtime::EpochRuntimeProjectionPort projection_{runtime_};
  engine::SimulationCoordinatorService coordinator_;
  engine::WindStoreCheckpointCatalog catalog_{metadata_};
  session::files::CatalogCheckpointStore checkpoints_{catalog_};
  std::unique_ptr<session::files::WindStoreRecordStore> records_;
  std::array<EngineOwner,engine::kMaxEngines> engines_{};
  runtime::EpochObservationCodec codec_;
  runtime::ObservationCheckpointService observation_checkpoint_;
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
      if(metadata_.is_segmented())return metadata_.can_append(32+engines*16,(32+engines*16)*16384)&&
        worker_.has_capacity(engines*5+16,(engines*5+16)*1024);
      constexpr auto cap=christeye::living::durable::kMaxEvents;
      const auto accepted=worker_.status().accepted;
      const auto reserve=32+engines*16;
      return metadata_.event_count()+reserve<cap&&
        initial_observations_+accepted+engines*5+16<cap;
    }
    session::StorageView storage()const noexcept override{
      const auto m=metadata_.capacity();const auto e=worker_.storage_capacity();
      return {e.known,m.segmented,e.writable,m.total_events,e.total_events,
        std::min(m.available_disk_bytes,e.available_bytes),m.archived_segments+e.archived_segments,
        m.resident_events,static_cast<std::size_t>(e.resident_events),m.event_limit};
    }
  };
  std::unique_ptr<Budget> budget_;
  std::unique_ptr<session::NativeSessionService> service_;
  std::unique_ptr<SessionExperimentRuntime> experiments_;
  std::unique_ptr<operations::WindStoreOperationStore> operation_store_;
  std::unique_ptr<session::OperationService> operations_;
  std::string error_;
  void on_service_event(const contracts::ServiceEvent& event)noexcept override{
    auto correlated=event;
    if(!correlated.operation_id&&service_)correlated.operation_id=service_->operation_id();
    for(std::size_t i=0;i<config_.count;++i)if(config_.engines[i].id==event.subject){
      correlated.source=config_.engines[i].source;correlated.source_epoch=config_.engines[i].epoch;break;
    }
    trace_.on_service_event(correlated);
    if(extension_)extension_->on_service_event(correlated);
  }
  bool bind_identity(bool manifest_exists){
    namespace db=christeye::living::durable;
    db::Event event{};bool found=false;
    if(!metadata_.latest_event("CULTURE_CHECKPOINT","native-session-root","identity",event,found,"anaiero-session-host"))return false;
    const auto identity="CEVSESSIONIDENTITY/1\n"+config_.name+"\n"+config_.digest+"\n";
    if(found){std::string value;return metadata_.get(event.object_hash,value)&&value==identity;}
    bool foreign=false;
    if(!metadata_.visit_events(0,[&](const db::Event& e){
      if(e.producer=="anaiero-session"&&e.entity!="session-"+config_.name)foreign=true;
      return !foreign;
    })||foreign||(!manifest_exists&&observations_.event_count()))return false;
    const auto object=metadata_.put(identity);
    return object.code==db::Code::ok&&metadata_.publish("CULTURE_CHECKPOINT","native-session-root",object.hash,
      "SIMULATED",1,"identity","anaiero-session-host").code==db::Code::ok;
  }
public:
  explicit NativeSessionApplication(runtime::IAsyncCompletionSignal* observer=nullptr):wakeup_(observer){}
  NativeSessionApplication(const NativeSessionApplication&)=delete;
  NativeSessionApplication& operator=(const NativeSessionApplication&)=delete;
  bool open(const std::filesystem::path& configuration,const std::filesystem::path& root){
    namespace db=christeye::living::durable;
    if(service_)return false;
    auto fail=[&](std::string message){error_=std::move(message);return false;};
    if(!session::files::read_configuration(configuration,config_,error_))return false;
    const auto options=config_.sustained_storage?db::StorageOptions::sustained(config_.segment_events):db::StorageOptions{};
    if(metadata_.open(root/"session",options)!=db::Code::ok||observations_.open(root/"evidence",options)!=db::Code::ok)
      return fail("session storage is unavailable, locked, or corrupt");
    records_=std::make_unique<session::files::WindStoreRecordStore>(metadata_,"session-"+config_.name);
    auto saved=std::make_unique<session::Record>();bool exists=false;
    if(!records_->load(*saved,exists))return fail("invalid session recovery manifest");
    if(exists&&(saved->configuration_sha256!=config_.digest||saved->coordinator.engine_count!=config_.count))
      return fail("session configuration/model identity changed");
    if(!bind_identity(exists))return fail("state directory belongs to a different session or configuration");
    experiments_=std::make_unique<SessionExperimentRuntime>(metadata_);
    experiments_->set_extension(this);coordinator_.set_extension(this);runtime_.set_extension(this);
    std::array<engine::ISimulationEngine*,engine::kMaxEngines> generic{};
    std::array<session::ISw5Port*,engine::kMaxEngines> sw5{};
    for(std::size_t i=0;i<config_.count;++i){
      const auto& cfg=config_.engines[i];auto& owner=engines_[i];
      owner.binding=SessionEngineFactoryRegistry::create(cfg,root/("engine-"+std::to_string(cfg.id)),error_);
      if(!owner.binding)return false;
      owner.binding->set_extension(this);
      auto& engine=owner.binding->engine();
      if(engine.initialize()!=engine::Status::ok)return fail("engine initialize failed: "+owner.binding->diagnostics());
      engine::CheckpointRecord checkpoint{};std::string bytes;
      const bool has_checkpoint=catalog_.latest(cfg.id,cfg.epoch,checkpoint,bytes);
      if(exists&&!has_checkpoint)return fail("missing engine checkpoint");
      if(has_checkpoint){
        if(checkpoint.source!=cfg.source||checkpoint.model_sha256!=engine.describe().model_digest)
          return fail("checkpoint identity mismatch");
        if(engine.describe().supports_restore){
          infrastructure::MemoryCheckpoint data;
          if(!data.write(bytes)||engine.restore(data)!=engine::Status::ok)return fail("engine checkpoint restore failed");
        }
      }
      owner.durable=std::make_unique<session::CheckpointingEngineService>(engine,checkpoints_);
      owner.durable->set_extension(this);
      if(!exists&&!owner.durable->persist_latest())return fail("initial engine checkpoint failed");
      if(exists&&saved->coordinator.cycle_active&&!saved->coordinator.publication_pending&&
         saved->coordinator.next_slot==i){
        const auto& pending=saved->coordinator.engines[i];
        owner.durable->authorize_replay({pending.last.tick,pending.cycle_inputs,saved->coordinator.completed_cycles+1});
      }
      if(!coordinator_.register_engine(*owner.durable,cfg.cadence))return fail("engine registration failed");
      generic[i]=owner.durable.get();
      owner.sw5=owner.binding->make_sw5_port(*owner.durable);sw5[i]=owner.sw5.get();
      if(owner.sw5&&!experiments_->bind_sw5(*owner.durable,*owner.sw5))return fail("SW5 experiment binding failed");
      if(auto* runner=owner.binding->sim3_runner();runner&&
         !experiments_->bind_sim3(*owner.durable,runner->configuration()))return fail("SIM3 experiment binding failed");
      const auto clock=config_.sustained_storage?engine.describe().capabilities.clock_domain:1u;
      if(!projection_.enroll({cfg.source,clock,runtime::Origin::simulated,true,false,false,0}))
        return fail("source enrollment failed");
    }
    journal_=std::make_unique<runtime::WindStoreEpochObservationJournal>(observations_,config_.name,&observation_checkpoint_);
    runtime::EpochObservationDeliveryService replay(codec_,*journal_,projection_);
    if(!replay.recover())return fail("evidence replay failed");
    worker_=std::make_unique<runtime::AsyncEpochJournalWorker<32>>(*journal_,codec_,&wakeup_);
    budget_=std::make_unique<Budget>(metadata_,*worker_,observations_.event_count());
    evidence_=std::make_unique<runtime::AsyncObservationCommitService>(*worker_,projection_);
    service_=std::make_unique<session::NativeSessionService>(config_,coordinator_,*records_,*evidence_,projection_,generic,sw5);
    service_->set_view_encoder(&view_encoder_);
    service_->set_experiments(experiments_.get());
    service_->set_cancellation(this);
    service_->set_extension(this);
    service_->set_trace(&trace_);
    service_->set_budget(budget_.get());
    if(!service_->start(std::move(*saved),exists))return fail("session continuation failed: "+service_->state_json());
    operation_store_=std::make_unique<operations::WindStoreOperationStore>(metadata_,"operations-"+config_.name);
    operations_=std::make_unique<session::OperationService>(*service_,*operation_store_);
    operations_->set_view_encoder(&view_encoder_);
    operations_->set_extension(this);
    if(!operations_->start())return fail("operation ledger recovery failed");
    return true;
  }
  void request_cancel()noexcept override{for(auto& owner:engines_)if(owner.binding)owner.binding->cancel();}
  // Configure before open or before handing the owner to SessionHostWorker.
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  session::NativeSessionService& service()noexcept{return *service_;}
  session::IDriver& driver()noexcept{return *operations_;}
  session::SessionWakeup& wakeup()noexcept{return wakeup_;}
  const session::Configuration& configuration()const noexcept{return config_;}
  const std::string& error()const noexcept{return error_;}
  bool settle(std::chrono::milliseconds timeout=std::chrono::seconds(30)){
    const auto deadline=std::chrono::steady_clock::now()+timeout;
    while(operations_->busy()&&!operations_->failed()){
      const auto observed=wakeup_.generation();
      if(!operations_->pump()){
        if(std::chrono::steady_clock::now()>=deadline)return false;
        wakeup_.wait(observed,deadline);
      }
    }
    return !operations_->failed();
  }
  std::size_t committed_observations_after_shutdown(){worker_->shutdown();return observations_.event_count();}
  // Offline owner operation: the host must not be running while this is called.
  bool maintain_storage(christeye::living::durable::ArchiveStats& metadata,
      christeye::living::durable::ArchiveStats& evidence){
    worker_->shutdown();
    if(!metadata_.is_segmented()||!observations_.is_segmented())return false;
    return records_->checkpoint_now()&&journal_->checkpoint_now()&&
      metadata_.collect_objects(metadata)==christeye::living::durable::Code::ok&&
      observations_.collect_objects(evidence)==christeye::living::durable::Code::ok;
  }
};
} // namespace anaiero::application
