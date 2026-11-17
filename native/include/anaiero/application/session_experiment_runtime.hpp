#pragma once
// Composition of immutable source readers, engine-specific trial adapters, and
// durable experiment progress. No owning session or live route is mutated here.
#include "anaiero/contracts/session_experiment.hpp"
#include "anaiero/contracts/native_session.hpp"
#include "anaiero/application/sw5_experiment_adapter.hpp"
#include "anaiero/application/sim3_experiment_adapter.hpp"
#include "anaiero/infrastructure/experiment_files.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/memory_checkpoint.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
namespace anaiero::application {
class SessionExperimentRuntime final:public session::IExperimentRuntime {
  struct Binding {
    engine::ISimulationEngine* engine=nullptr;
    session::ISw5Port* sw5=nullptr;
    const christeye::living::sim3::Configuration* sim3=nullptr;
  };
  christeye::living::durable::Registry& registry_;
  experiments::files::WindStoreExperimentStore store_;
  std::array<Binding,engine::kMaxEngines> bindings_{};
  std::size_t count_=0;
  cells::BoundedPositionPort isolated_spatial_{};
  std::unique_ptr<experiments::IEvaluator> evaluator_;
  std::unique_ptr<experiments::ExperimentService> service_;
  contracts::IServiceExtension* extension_=nullptr;
  experiments::Code code_=experiments::Code::ok;
  std::string error_;
  static std::string run_id(std::uint64_t id){return "experiment-"+std::to_string(id);}
  Binding* binding(std::uint32_t engine)noexcept {
    for(std::size_t i=0;i<count_;++i)if(bindings_[i].engine->describe().id==engine)return &bindings_[i];
    return nullptr;
  }
  bool bind(Binding value)noexcept {
    if(!value.engine||count_==bindings_.size()||service_||!value.engine->describe().id||
       binding(value.engine->describe().id))return false;
    bindings_[count_++]=value;return true;
  }
  bool evaluator(const experiments::Definition& d,std::string_view checkpoint)noexcept {
    auto* source=binding(d.engine_id);
    if(!source){code_=experiments::Code::unsupported;return false;}
    const auto descriptor=source->engine->describe();
    std::unique_ptr<experiments::IEvaluator> next;
    if(d.kind==engine::Kind::sw5&&source->sw5){
      cells::World world{};
      if(cells::files::deserialize_world(checkpoint,source->sw5->atlas(),world)!=cells::Status::ok){
        code_=experiments::Code::identity_mismatch;return false;
      }
      next=std::make_unique<Sw5ExperimentAdapter>(descriptor,source->sw5->atlas(),world,isolated_spatial_);
    }else if(d.kind==engine::Kind::sim3&&source->sim3){
      models::sim3::Snapshot snapshot{};
      if(christeye::living::sim3::decode_snapshot(checkpoint,*source->sim3,snapshot)!=models::sim3::Result::ok){
        code_=experiments::Code::identity_mismatch;return false;
      }
      next=std::make_unique<Sim3ExperimentAdapter>(descriptor,*source->sim3,std::move(snapshot));
    }else {code_=experiments::Code::unsupported;return false;}
    experiments::EvaluationContext context{};code_=next->context(d,context);
    if(code_!=experiments::Code::ok)return false;
    service_.reset();evaluator_=std::move(next);
    service_=std::make_unique<experiments::ExperimentService>(store_,*evaluator_);
    service_->set_extension(extension_);return true;
  }
public:
  explicit SessionExperimentRuntime(christeye::living::durable::Registry& registry):registry_(registry),store_(registry){}
  bool bind_sw5(engine::ISimulationEngine& engine,session::ISw5Port& port)noexcept {
    return engine.describe().kind==engine::Kind::sw5&&bind({&engine,&port,nullptr});
  }
  bool bind_sim3(engine::ISimulationEngine& engine,const christeye::living::sim3::Configuration& config)noexcept {
    return engine.describe().kind==engine::Kind::sim3&&bind({&engine,nullptr,&config});
  }
  void set_extension(contracts::IServiceExtension* extension)noexcept override {
    extension_=extension;if(service_)service_->set_extension(extension);
  }
  bool start(std::uint64_t operation_id,std::string_view reference)noexcept override {
    if(!operation_id||reference.empty()||reference.size()>4096||busy()){code_=experiments::Code::invalid_state;return false;}
    // The operation ledger pins the command reference. Recovery uses the already
    // committed definition even when its original source file has moved.
    experiments::Record saved{};bool exists=false;
    if(!store_.load(run_id(operation_id),saved,exists)){code_=experiments::Code::persistence_failed;return false;}
    if(exists)return recover(operation_id);
    experiments::Definition definition{};const auto at=reference.rfind('@');
    if(at!=std::string_view::npos){
      if(!experiments::files::lookup_definition(registry_,reference.substr(0,at),reference.substr(at+1),definition)){
        code_=experiments::Code::invalid_definition;return false;
      }
    }else if(!experiments::files::register_definition(registry_,std::filesystem::path(reference),definition,error_)){
      code_=experiments::Code::invalid_definition;return false;
    }
    auto* source=binding(definition.engine_id);
    if(!source){code_=experiments::Code::unsupported;return false;}
    infrastructure::MemoryCheckpoint checkpoint;engine::Snapshot observed{};
    if(source->engine->checkpoint(checkpoint)!=engine::Status::ok||source->engine->latest(observed)!=engine::Status::ok||
       christeye::living::plans::sha256_hex(checkpoint.bytes())!=experiments::files::digest_view(observed)){
      code_=experiments::Code::identity_mismatch;return false;
    }
    if(!evaluator(definition,checkpoint.bytes()))return false;
    experiments::EvaluationContext context{};code_=evaluator_->context(definition,context);
    if(code_!=experiments::Code::ok||context.start_tick!=observed.tick||context.source_revision!=observed.revision){
      code_=experiments::Code::identity_mismatch;return false;
    }
    code_=service_->start(run_id(operation_id),definition,std::string(checkpoint.bytes()));
    return code_==experiments::Code::ok;
  }
  bool recover(std::uint64_t operation_id)noexcept override {
    if(!operation_id){code_=experiments::Code::invalid_state;return false;}
    experiments::Record saved{};bool exists=false;
    if(!store_.load(run_id(operation_id),saved,exists)){code_=experiments::Code::persistence_failed;return false;}
    if(!exists){code_=experiments::Code::invalid_state;return false;}
    if(!evaluator(saved.definition,saved.source_checkpoint))return false;
    code_=service_->recover(run_id(operation_id));return code_==experiments::Code::ok;
  }
  bool pump()noexcept override {
    if(!service_||!service_->busy())return false;
    code_=service_->step();return code_==experiments::Code::ok||code_==experiments::Code::cancelled;
  }
  bool cancel()noexcept override {
    if(!service_)return false;
    if(service_->record().state==experiments::RunState::paused)return true;
    code_=service_->pause();return code_==experiments::Code::cancelled;
  }
  bool resume()noexcept override {
    if(!service_)return false;
    // A session may restart after the experiment's running revision committed
    // but before the enclosing operation/session stamp. Reconcile that intent
    // without another revision or replaying any completed trial.
    if(service_->busy()){code_=experiments::Code::ok;return true;}
    code_=service_->resume();return code_==experiments::Code::ok;
  }
  bool busy()const noexcept override{return service_&&service_->busy();}
  bool failed()const noexcept override {
    return code_==experiments::Code::persistence_failed||code_==experiments::Code::corrupt||
      (service_&&service_->record().state==experiments::RunState::failed);
  }
  const experiments::Record* record()const noexcept override{return service_?&service_->record():nullptr;}
  experiments::Code code()const noexcept override{return code_;}
  const std::string& error()const noexcept{return error_;}
  bool mark_applied(std::uint64_t id,std::uint64_t tick)noexcept override {
    if(!service_)return false;
    code_=service_->mark_applied(id,tick);return code_==experiments::Code::ok;
  }
  bool observe(const engine::Snapshot& snapshot)noexcept override {
    if(!service_)return false;
    code_=service_->observe(snapshot);return code_==experiments::Code::ok;
  }
};
} // namespace anaiero::application
