#pragma once
// Small host-domain command service. Deliberately no filesystem or OS API.
#include "anaiero/contracts/host_runtime.hpp"
#include "anaiero/contracts/runtime_events.hpp"
#include "anaiero/contracts/service_extension.hpp"
#include <cstdio>
#include <cstring>
#include <limits>
namespace anaiero::host {
class ControllerCommandService final:public ICommandService {
  runtime::IObservationRuntime& runtime_;
  contracts::IServiceExtension* extension_=nullptr;
  IDigitalCyclePort* cycle_=nullptr;
  std::uint64_t requests_=0;
  static Reply make(std::string_view bytes,bool stop=false) noexcept {
    Reply reply{};
    if(bytes.size()>=reply.bytes.size())return reply;
    std::memcpy(reply.bytes.data(),bytes.data(),bytes.size());
    reply.length=bytes.size();reply.stop=stop;return reply;
  }
public:
  explicit ControllerCommandService(runtime::IObservationRuntime& runtime) noexcept:
    runtime_(runtime){}
  void set_extension(contracts::IServiceExtension* ext) noexcept {extension_=ext;}
  void set_cycle(IDigitalCyclePort* cycle) noexcept {cycle_=cycle;}
  Reply handle(std::string_view command) noexcept override {
    ++requests_;
    Reply reply{};
    if(command=="PING")reply=make("PONG\n");
    else if(command=="SNAPSHOT"||command=="STATE"){
      const auto snapshot=runtime_.latest(0,std::numeric_limits<std::uint64_t>::max());
      char text[256]{};
      const int count=std::snprintf(text,sizeof(text),
        "{\"schema\":\"anaiero.host.status/1\",\"revision\":%llu,"
        "\"metrics\":%u,\"pending\":%llu}\n",
        static_cast<unsigned long long>(snapshot.revision),
        static_cast<unsigned>(snapshot.count),
        static_cast<unsigned long long>(runtime_.pending()));
      if(count>0&&static_cast<std::size_t>(count)<sizeof(text))
        reply=make(std::string_view(text,static_cast<std::size_t>(count)));
    }else if(command=="STEP"){
      reply=make(cycle_&&cycle_->advance_one_simulated_tick()?
                 "OK STEP\n":"ERR step unavailable\n");
    }else if(command=="STOP")reply=make("BYE\n",true);
    else reply=make("ERR unknown command\n");
    if(extension_)extension_->on_service_event(
      {contracts::ServiceId::runtime_dispatch,contracts::ServicePhase::reviewed,
       requests_,0,reply.stop?1:0});
    return reply;
  }
  std::uint64_t requests()const noexcept{return requests_;}
};
} // namespace anaiero::host
