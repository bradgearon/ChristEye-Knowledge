// Anaiero native Windows/Linux headless local controller.
#include "anaiero/infrastructure/native_ipc.hpp"
#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/services/runtime_service.hpp"
#include "anaiero/services/controller_command_service.hpp"
#include "anaiero/application/sim3_controller_cycle.hpp"
#include <iostream>
#include <filesystem>
#include <string>
#include <string_view>
namespace host=anaiero::host;
namespace rt=anaiero::runtime;
namespace sim3=christeye::living::sim3;
int run_session_host(int argc,char**argv);
int run_host_command(int argc,char**argv) {
  if(argc>=2&&(std::string_view(argv[1])=="session"||std::string_view(argv[1])=="service"))
    return run_session_host(argc,argv);
  if(argc==5&&std::string_view(argv[1])=="start"&&std::filesystem::path(argv[3]).extension()==".cevsession")
    return run_session_host(argc,argv);
  if(argc>=2&&std::string_view(argv[1])=="query"){
    if(argc!=4)return 2;
    const auto command=std::string_view(argv[3]);
    if(command.empty()||command.size()>4090||command.find('\n')!=command.npos||
       command.find('\r')!=command.npos)return 2;
    std::string response;
    if(!host::NativeIpcClient::request(argv[2],command,response))return 1;
    std::cout<<response;
    return response.starts_with("ERR")?1:0;
  }
  if(argc!=5||std::string_view(argv[1])!="start"){
    std::cerr<<"anaiero runtime start <endpoint> <sim3.cev> <state-dir>\n"
             <<"anaiero runtime query <endpoint> \"COMMAND [arguments]\"\n"
             <<"anaiero runtime session <session.cevsession> <state-dir> [cycles]\n"
             <<"anaiero runtime service <endpoint> <session.cevsession> <state-dir>\n";
    return 2;
  }
  if(!host::valid_endpoint(argv[2],
#ifdef _WIN32
                           true
#else
                           false
#endif
                           ))return 2;
  rt::WeightedQueue<> queue;
  rt::RuntimeService runtime(queue);
  sim3::Runner runner;
  if(runner.open(argv[3],argv[4])!=sim3::Result::ok){
    std::cerr<<"anaiero host: unable to load durable SIM3 session\n";return 1;
  }
  rt::Sim3Ingress ingress(runtime);
  if(ingress.enroll(runner.configuration())!=rt::Status::ok||
     ingress.import(runner.configuration(),runner.state())!=rt::Status::ok)return 1;
  while(runtime.pending())runtime.drain(64);
  host::ControllerCommandService controller(runtime);
  anaiero::application::Sim3ControllerCycle digital(runner,ingress,runtime);
  controller.set_cycle(&digital);
  host::NativeIpcServer ipc(argv[2]);
  std::cout<<"Anaiero native controller ready (local authenticated endpoint)\n";
  std::cout.flush();
  const auto result=ipc.serve(controller);
  return result==host::Code::ok?0:1;
}
