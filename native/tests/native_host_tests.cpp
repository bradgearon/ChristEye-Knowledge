// Native local IPC: framing/deadlines, client-fault isolation, cancellation,
// endpoint ownership, full state responses and process lifecycle hooks.
#include "anaiero/infrastructure/native_ipc.hpp"
#include "anaiero/infrastructure/native_host_lifecycle.hpp"
#include "anaiero/services/controller_command_service.hpp"
#include "anaiero/services/runtime_service.hpp"
#include "anaiero/infrastructure/weighted_queue.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>
#define CHECK(e) do { if(!(e)){std::cerr<<"FAILED " #e <<" line "<<__LINE__<<"\n";std::abort();} } while(false)
namespace host=anaiero::host;
namespace rt=anaiero::runtime;
using namespace std::chrono_literals;
struct DigitalCycle final:host::IDigitalCyclePort {
  rt::IObservationRuntime& runtime;
  std::uint64_t next=2;
  explicit DigitalCycle(rt::IObservationRuntime& r)noexcept:runtime(r){}
  bool advance_one_simulated_tick() noexcept override {
    rt::Observation e{};
    e.colony=1;e.source=1;e.clock_domain=1;
    e.metric=rt::Metric::energy;e.origin=rt::Origin::simulated;
    e.lane=rt::Lane::health;e.value=110;e.sequence=next++;
    e.acquired_ns=2000000;
    if(runtime.post(e)!=rt::Status::ok)return false;
    while(runtime.pending())runtime.drain(64);
    return runtime.pending()==0;
  }
};
struct Commands final:host::ICommandService {
  host::ControllerCommandService original;
  explicit Commands(rt::IObservationRuntime& runtime):original(runtime){}
  host::Reply handle(std::string_view command)noexcept override {
    if(command=="LARGE"){
      host::Reply result{};
      result.length=12001;
      std::fill_n(result.bytes.begin(),12000,'x');
      result.bytes[12000]='\n';return result;
    }
    if(command=="DELAY"){
      std::this_thread::sleep_for(600ms);
      host::Reply result{};result.bytes[0]='\n';result.length=1;return result;
    }
    return original.handle(command);
  }
};
std::string make_endpoint(std::string_view suffix) {
#ifdef _WIN32
  const auto id=static_cast<unsigned long long>(GetCurrentProcessId());
  return "anaiero-"+std::to_string(id)+"-"+std::string(suffix);
#else
  const auto id=static_cast<unsigned long long>(getpid());
  return (std::filesystem::temp_directory_path()/
    ("anaiero-"+std::to_string(id)+"-"+std::string(suffix)+".sock")).string();
#endif
}
void await_ready(host::NativeIpcServer& server) {
  const auto until=std::chrono::steady_clock::now()+5s;
  while(!server.ready()&&std::chrono::steady_clock::now()<until)
    std::this_thread::sleep_for(2ms);
  CHECK(server.ready());
}
bool send(const std::string& endpoint,std::string_view command,std::string& response) {
  const auto until=std::chrono::steady_clock::now()+5s;
  do{
    if(host::NativeIpcClient::request(endpoint,command,response))return true;
    std::this_thread::sleep_for(2ms);
  }while(std::chrono::steady_clock::now()<until);
  return false;
}
class RawClient {
#ifdef _WIN32
  HANDLE descriptor_=INVALID_HANDLE_VALUE;
#else
  int descriptor_=-1;
#endif
public:
  explicit RawClient(const std::string& endpoint) {
#ifdef _WIN32
    const auto name=host::native_io::pipe_name(endpoint);
    const auto until=std::chrono::steady_clock::now()+5s;
    do{
      descriptor_=CreateFileA(name.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,
        OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);
      if(descriptor_!=INVALID_HANDLE_VALUE)break;
      WaitNamedPipeA(name.c_str(),100);
    }while(std::chrono::steady_clock::now()<until);
    CHECK(descriptor_!=INVALID_HANDLE_VALUE);
#else
    descriptor_=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC|SOCK_NONBLOCK,0);
    CHECK(descriptor_>=0);
    sockaddr_un address{};address.sun_family=AF_UNIX;
    std::memcpy(address.sun_path,endpoint.c_str(),endpoint.size()+1);
    CHECK(connect(descriptor_,reinterpret_cast<sockaddr*>(&address),sizeof(address))==0);
#endif
  }
  RawClient(const RawClient&)=delete;
  RawClient&operator=(const RawClient&)=delete;
  ~RawClient(){
#ifdef _WIN32
    if(descriptor_!=INVALID_HANDLE_VALUE)CloseHandle(descriptor_);
#else
    if(descriptor_>=0)close(descriptor_);
#endif
  }
  bool write(std::string_view bytes) {
    host::native_io::Deadline deadline(1000);
#ifdef _WIN32
    return host::native_io::write_all(descriptor_,bytes,nullptr,deadline);
#else
    return host::native_io::write_all(descriptor_,bytes,-1,nullptr,deadline);
#endif
  }
};
int main() {
#ifndef _WIN32
  const int probe=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);
  if(probe<0&&(errno==EPERM||errno==EACCES)){
    std::cout<<"UNAVAILABLE: this executor forbids local sockets; IPC/lifecycle execution requires native CI\n";
    return std::getenv("ANAIERO_REQUIRE_IPC")?1:77;
  }
  CHECK(probe>=0);close(probe);
#endif
  CHECK(!host::valid_endpoint("../bad",true));
  CHECK(!host::valid_endpoint("",false));
  CHECK(!host::native_io::valid_command("PING\nSTOP"));
  CHECK(!host::native_io::valid_command(std::string(host::kMaxRequestBytes+1,'x')));
  rt::WeightedQueue<> queue;
  rt::RuntimeService runtime(queue);
  CHECK(runtime.enroll({1,1,rt::Origin::simulated,true,false,false})==rt::Status::ok);
  rt::Observation e{};e.colony=1;e.source=1;e.clock_domain=1;
  e.metric=rt::Metric::energy;e.lane=rt::Lane::health;e.value=100;
  e.sequence=1;e.acquired_ns=1000000;
  CHECK(runtime.post(e)==rt::Status::ok);
  runtime.drain();
  Commands commands(runtime);
  DigitalCycle digital(runtime);commands.original.set_cycle(&digital);
  const auto endpoint=make_endpoint("resilience");
  host::NativeIpcServer server(endpoint,200);
  host::Code completed=host::Code::interrupted;
  std::thread worker([&]{completed=server.serve(commands);});
  await_ready(server);
  std::string response;
  CHECK(send(endpoint,"PING",response)&&response=="PONG\n");
  CHECK(send(endpoint,"STATE",response)&&response.find("\"revision\":1")!=std::string::npos);
  CHECK(send(endpoint,"STEP",response)&&response=="OK STEP\n");
  CHECK(send(endpoint,"LARGE",response)&&response.size()==12001&&response.back()=='\n');

  {RawClient disconnected(endpoint);}
  CHECK(send(endpoint,"PING",response)&&response=="PONG\n");
  {RawClient malformed(endpoint);malformed.write("PING\r\n");}
  CHECK(send(endpoint,"PING",response)&&response=="PONG\n");
  {RawClient oversized(endpoint);oversized.write(std::string(host::kMaxRequestBytes+2,'x'));}
  CHECK(send(endpoint,"PING",response)&&response=="PONG\n");
  {RawClient abandoned(endpoint);abandoned.write("LARGE\n");}
  CHECK(send(endpoint,"PING",response)&&response=="PONG\n");

  // Bytes arriving individually may not renew the absolute 200ms deadline.
  {
    RawClient slow(endpoint);
    unsigned accepted=0;
    for(unsigned attempt=0;attempt<10;++attempt){
      if(!slow.write("x"))break;
      ++accepted;std::this_thread::sleep_for(70ms);
    }
    CHECK(accepted<8);
  }
  CHECK(send(endpoint,"PING",response)&&response=="PONG\n");

  // A second owner must fail without removing the active owner's endpoint.
  {
    host::NativeIpcServer collision(endpoint,200);
    CHECK(collision.serve(commands,1)==host::Code::bind_failure);
  }
  CHECK(send(endpoint,"PING",response)&&response=="PONG\n");

  // Client deadline also applies while waiting for a slow application reply.
  const auto started=std::chrono::steady_clock::now();
  CHECK(!host::NativeIpcClient::request(endpoint,"DELAY",response,100));
  CHECK(std::chrono::steady_clock::now()-started<500ms);
  CHECK(send(endpoint,"PING",response)&&response=="PONG\n");

  // Cancel an in-flight incomplete frame, not just an idle listener.
  {
    RawClient stalled(endpoint);CHECK(stalled.write("P"));
    const auto stop_started=std::chrono::steady_clock::now();
    server.request_stop();
    worker.join();
    CHECK(std::chrono::steady_clock::now()-stop_started<1s);
  }
  CHECK(completed==host::Code::interrupted);
  CHECK(runtime.latest(0,UINT64_MAX).revision==2);

  {
    host::NativeIpcServer idle(make_endpoint("idle"));
    std::thread waiting([&]{completed=idle.serve(commands);});
    await_ready(idle);
    idle.request_stop();waiting.join();
    CHECK(completed==host::Code::interrupted);
  }
  {
    host::NativeIpcServer lifecycle(make_endpoint("lifecycle"));
    std::atomic<unsigned> ready{0},stopped{0};
    host::NativeLifecycleOptions options;
    options.on_ready=[](void* p)noexcept{++*static_cast<std::atomic<unsigned>*>(p);};
    options.ready_context=&ready;
    options.on_stop=[](void* p)noexcept{++*static_cast<std::atomic<unsigned>*>(p);};
    options.stop_context=&stopped;
    std::thread managed([&]{completed=host::NativeHostLifecycle::run(lifecycle,commands,options);});
    await_ready(lifecycle);
    CHECK(send(make_endpoint("lifecycle"),"STOP",response)&&response=="BYE\n");
    managed.join();
    CHECK(completed==host::Code::ok&&ready.load()==1&&stopped.load()==1);
  }
#ifndef _WIN32
  {
    host::NativeIpcServer signaled(make_endpoint("signal"));
    std::thread managed([&]{completed=host::NativeHostLifecycle::run(signaled,commands);});
    await_ready(signaled);
    CHECK(kill(getpid(),SIGTERM)==0);
    managed.join();CHECK(completed==host::Code::interrupted);
  }
#endif
  std::cout<<"native IPC framing, client isolation, deadlines, ownership, stop and lifecycle PASS\n";
}
