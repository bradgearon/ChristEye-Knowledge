// Cross-platform local authenticated IPC with deterministic simulated tick.
#include "anaiero/infrastructure/native_ipc.hpp"
#include "anaiero/services/controller_command_service.hpp"
#include "anaiero/services/runtime_service.hpp"
#include "anaiero/infrastructure/weighted_queue.hpp"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#define CHECK(e) do { if(!(e)){std::cerr<<"FAILED " #e <<" line "<<__LINE__<<"\n";std::abort();} } while(false)
namespace host=anaiero::host;
namespace rt=anaiero::runtime;
struct DigitalCycle final:host::IDigitalCyclePort {
  rt::IObservationRuntime& runtime;
  std::uint64_t next=2;
  explicit DigitalCycle(rt::IObservationRuntime& r)noexcept:runtime(r){}
  bool advance_one_simulated_tick() noexcept override {
    rt::Observation e{};
    e.colony=1;e.animon=0;e.source=1;e.clock_domain=1;
    e.metric=rt::Metric::energy;e.origin=rt::Origin::simulated;
    e.lane=rt::Lane::health;e.value=110;e.sequence=next++;
    e.acquired_ns=2000000;
    if(runtime.post(e)!=rt::Status::ok)return false;
    while(runtime.pending())runtime.drain(64);
    return runtime.pending()==0;
  }
};
int main() {
  const auto id=
#ifdef _WIN32
    static_cast<unsigned long long>(GetCurrentProcessId());
  std::string endpoint="anaiero-host-"+std::to_string(id);
#else
    static_cast<unsigned long long>(getpid());
  std::string endpoint=(std::filesystem::temp_directory_path()/
    ("anaiero-host-"+std::to_string(id)+".sock")).string();
#endif
  CHECK(!host::valid_endpoint("../bad",true));
  CHECK(!host::valid_endpoint("",false));
  rt::WeightedQueue<> queue;
  rt::RuntimeService runtime(queue);
  CHECK(runtime.enroll({1,1,rt::Origin::simulated,true,false,false})==rt::Status::ok);
  rt::Observation e{};e.colony=1;e.source=1;e.clock_domain=1;
  e.metric=rt::Metric::energy;e.lane=rt::Lane::health;e.value=100;
  e.sequence=1;e.acquired_ns=1000000;
  CHECK(runtime.post(e)==rt::Status::ok);
  runtime.drain();
  host::ControllerCommandService commands(runtime);
  DigitalCycle digital(runtime);commands.set_cycle(&digital);
  host::NativeIpcServer server(endpoint);
  host::Code completed=host::Code::interrupted;
  std::thread worker([&]{completed=server.serve(commands,4);});
  const auto send=[&](std::string_view query,std::string& reply){
    for(int retry=0;retry<200;++retry){
      if(host::NativeIpcClient::request(endpoint,query,reply))return true;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
  };
  std::string response;
  CHECK(send("PING",response)&&response=="PONG\n");
  CHECK(send("STATE",response)&&response.find("\"revision\":1")!=std::string::npos);
  CHECK(send("STEP",response)&&response=="OK STEP\n");
  CHECK(send("STOP",response)&&response=="BYE\n");
  worker.join();
  CHECK(completed==host::Code::ok&&commands.requests()==4);
  CHECK(runtime.latest(0,UINT64_MAX).revision==2);
  std::cout<<"native local auth IPC and digital host command tests PASS\n";
}
