#pragma once
#include "anaiero/infrastructure/native_child_process.hpp"
#include "anaiero/infrastructure/supervised_solver_exchange.hpp"
#include "anaiero/services/scientific_engine_service.hpp"
namespace anaiero::application {
class SupervisedScientificEngine final:public engine::ISimulationEngine {
  science::ScientificConfiguration config_;
  process::NativeChildProcess process_;
  science::SupervisedSolverExchange exchange_;
  science::ScientificEngineService service_;
  bool started_=false;
  static science::SolverIdentity identity(science::SolverIdentity value,std::uint64_t epoch){value.epoch=epoch;return value;}
public:
  SupervisedScientificEngine(std::uint32_t id,std::uint32_t source,std::uint64_t epoch,science::ScientificConfiguration config):
    config_(std::move(config)),exchange_(process_,config_.timeout_ms),
    service_(exchange_,id,source,identity(config_.expected,epoch),config_.input_proxy){}
  engine::Descriptor describe()const noexcept override{return service_.describe();}
  engine::Status initialize()noexcept override{
    if(!started_){if(process_.start(config_.process)!=process::Code::ok)return engine::Status::io_failure;started_=true;}
    return service_.initialize();
  }
  engine::Status advance(const engine::StepRequest& request,engine::Snapshot& out)noexcept override{return service_.advance(request,out);}
  engine::Status latest(engine::Snapshot& out)const noexcept override{return service_.latest(out);}
  engine::Status checkpoint(engine::ICheckpointWriter& out)const noexcept override{return service_.checkpoint(out);}
  engine::Status restore(engine::ICheckpointReader& in)noexcept override{return service_.restore(in);}
  bool quantity(science::Quantity& out)const noexcept{return service_.quantity(out);}
  void set_extension(contracts::IServiceExtension* hook)noexcept{service_.set_extension(hook);}
  void cancel()noexcept{process_.cancel();}
  process::Code process_status()const noexcept{return process_.status();}
  std::string diagnostics()const{return process_.diagnostics();}
};
} // namespace anaiero::application
