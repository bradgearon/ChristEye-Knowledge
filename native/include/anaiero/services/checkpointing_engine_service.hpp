#pragma once
#include "anaiero/contracts/native_session.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <string>
namespace anaiero::session {
// Serial engine owner: checkpoint before the coordinator may publish a step.
// Replay authorization comes only from a validated saved coordinator intent.
class CheckpointingEngineService final:public engine::ISimulationEngine {
  engine::ISimulationEngine& inner_;
  IEngineCheckpointStore& store_;
  bool replay_allowed_=false;
  engine::StepRequest replay_{};
  contracts::IServiceExtension* extension_=nullptr;
  struct Writer final:engine::ICheckpointWriter {
    std::string data;
    bool write(std::string_view value)noexcept override {
      if(value.empty()||value.size()>1024*1024)return false;
      data.assign(value);return true;
    }
  };
  static bool same(const engine::StepRequest& a,const engine::StepRequest& b)noexcept {
    return a.expected_tick==b.expected_tick&&a.controller_cycle==b.controller_cycle&&
      a.inputs.support_proxy==b.inputs.support_proxy&&a.inputs.stress_proxy==b.inputs.stress_proxy&&
      a.inputs.digital_signal==b.inputs.digital_signal&&a.inputs.lesson==b.inputs.lesson;
  }
public:
  CheckpointingEngineService(engine::ISimulationEngine& inner,IEngineCheckpointStore& store)
    noexcept:inner_(inner),store_(store){}
  engine::Descriptor describe()const noexcept override{return inner_.describe();}
  engine::Status initialize()noexcept override{return inner_.initialize();}
  engine::Status latest(engine::Snapshot& s)const noexcept override{return inner_.latest(s);}
  engine::Status checkpoint(engine::ICheckpointWriter& w)const noexcept override{return inner_.checkpoint(w);}
  engine::Status restore(engine::ICheckpointReader& r)noexcept override{return inner_.restore(r);}
  void authorize_replay(const engine::StepRequest& step)noexcept {replay_=step;replay_allowed_=true;}
  void set_extension(contracts::IServiceExtension* extension)noexcept{extension_=extension;}
  bool persist_latest()noexcept {
    engine::Snapshot snap{};Writer writer;
    return latest(snap)==engine::Status::ok&&checkpoint(writer)==engine::Status::ok&&
      store_.save(describe(),snap,writer.data);
  }
  engine::Status advance(const engine::StepRequest& request,engine::Snapshot& out)noexcept override {
    engine::Snapshot actual{};
    if(inner_.latest(actual)!=engine::Status::ok)return engine::Status::not_initialized;
    if(actual.tick==request.expected_tick) {
      const auto status=inner_.advance(request,out);
      if(status!=engine::Status::ok)return status;
      authorize_replay(request);
    }else if(request.expected_tick!=UINT64_MAX&&actual.tick==request.expected_tick+1&&
             replay_allowed_&&same(request,replay_))out=actual;
    else return engine::Status::tick_conflict;
    if(!persist_latest())return engine::Status::io_failure;
    replay_allowed_=false;
    if(extension_)extension_->on_service_event({contracts::ServiceId::evidence,
      contracts::ServicePhase::committed,out.revision,out.id,0});
    return engine::Status::ok;
  }
};
} // namespace anaiero::session
