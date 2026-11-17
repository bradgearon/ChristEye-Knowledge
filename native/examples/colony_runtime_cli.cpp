#include "anaiero/infrastructure/weighted_queue.hpp"
#include "anaiero/services/runtime_service.hpp"
// anaiero runtime — low-resource host-side projector and simulated bus runner.
// This command does not provide physical cell care, network security or control.
#include "anaiero/infrastructure/sim3_ingress.hpp"
#include "anaiero/infrastructure/windstore_sink.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
namespace rt=anaiero::runtime;
namespace sim=christeye::living::sim3;
namespace durable=christeye::living::durable;
inline const char* metric_name(rt::Metric metric) noexcept {
  switch(metric){
    case rt::Metric::viability:return "viability";
    case rt::Metric::energy:return "energy";
    case rt::Metric::support:return "support";
    case rt::Metric::stress:return "stress";
    case rt::Metric::activity:return "activity";
    case rt::Metric::digital_learning:return "digital_learning";
    case rt::Metric::digital_desire:return "digital_desire";
    case rt::Metric::connection:return "connection";
    case rt::Metric::acquisition_health:return "acquisition_health";
    case rt::Metric::digital_route_active:return "digital_route_active";
  }
  return "invalid";
}
inline const char* quality_name(rt::Quality value) noexcept {
  switch(value){case rt::Quality::current:return "CURRENT";case rt::Quality::degraded:return "DEGRADED";
    case rt::Quality::stale:return "STALE";case rt::Quality::unknown:return "UNKNOWN";}
  return "UNKNOWN";
}
inline void print_latest(const rt::RuntimeService& runtime,std::uint64_t now_ns){
  const auto state=runtime.latest(now_ns,5000000ULL);
  std::cout<<"{\"schema\":\"anaiero.colony.latest/1\",\"origin\":\"SIMULATED_UNCALIBRATED\","
           <<"\"revision\":"<<state.revision<<",\"updates\":"<<state.committed_updates
           <<",\"sequence_gaps\":"<<state.sequence_gaps<<",\"pending\":"<<runtime.pending()
           <<",\"rejected\":"<<runtime.rejected()
           <<",\"commit_failures\":"<<runtime.commit_failures()<<",\"metrics\":[";
  for(std::size_t i=0;i<state.count;++i){
    const auto& v=state.values[i];if(i)std::cout<<',';
    std::cout<<"{\"colony\":"<<v.colony<<",\"animon\":"<<v.animon
             <<",\"metric\":\""<<metric_name(v.metric)<<"\",\"value\":"<<v.value
             <<",\"quality\":\""<<quality_name(v.quality)<<"\",\"source\":"<<v.source
             <<",\"origin\":\""<<(v.origin==rt::Origin::simulated?"SIMULATED":"EXTERNAL_UNVERIFIED_READONLY")
             <<"\",\"sequence\":"<<v.sequence
             <<",\"evidence_tag\":"<<v.evidence_tag
             <<",\"acquired_ns\":"<<v.acquired_ns
             <<",\"source_epoch\":"<<v.source_epoch<<"}";
  }
  std::cout<<"]}\n";
}
int run_host_command(int argc,char**argv);
int main(int argc,char**argv){
  if(argc>=2&&(std::string_view(argv[1])=="start"||
      std::string_view(argv[1])=="query"||std::string_view(argv[1])=="session"||
      std::string_view(argv[1])=="service"))return run_host_command(argc,argv);
  if(argc<4 || argc>5 ||
     (std::string_view(argv[1])!="simulate"&&std::string_view(argv[1])!="serve")){
    std::cerr<<"usage: anaiero runtime simulate|serve <run.cev> <state-dir> [ticks]\n"
      <<"simulate runs bounded synthetic ticks; serve waits for STEP / STATE / QUIT on stdin.\n";
    return 2;
  }
  const auto mode=std::string_view(argv[1]);
  if(mode=="serve"&&argc!=4)return 2;
  sim::Runner runner;
  const auto opened=runner.open(argv[2],argv[3]);
  if(opened!=sim::Result::ok){std::cerr<<"simulation open failed\n";return 1;}
  rt::WeightedQueue<> runtime_queue;rt::RuntimeService runtime(runtime_queue);
  rt::Sim3Ingress adapter(runtime);
  if(adapter.enroll(runner.configuration())!=rt::Status::ok)return 1;
  const auto observe=[&]()->bool{
    if(adapter.import(runner.configuration(),runner.state())!=rt::Status::ok)return false;
    while(runtime.pending())runtime.drain(64);
    return true;
  };
  if(!observe())return 1;
  if(mode=="simulate"){
    if(argc!=5)return 2;
    std::uint32_t ticks=0;
    if(!sim::parse_u32(argv[4],ticks)||ticks==0||ticks>256)return 2;
    for(std::uint32_t i=0;i<ticks;++i){
      if(runner.step(false)!=sim::Result::ok||!observe())return 1;
    }
    print_latest(runtime,runner.state().tick*1000000ULL);
    return 0;
  }
  print_latest(runtime,runner.state().tick*1000000ULL);
  // Event-driven local controller shell: input blocks; no status polling loop.
  // Real Linux IPC/epoll service transport and trusted sensor adapters are future work.
  std::string command;
  while(std::getline(std::cin,command)){
    if(command=="QUIT")break;
    if(command=="STATE"){
      print_latest(runtime,runner.state().tick*1000000ULL);continue;
    }
    if(command=="STEP"){
      if(runner.step(false)!=sim::Result::ok||!observe())return 1;
      print_latest(runtime,runner.state().tick*1000000ULL);continue;
    }
    std::cout<<"ERR unknown command; expected STEP, STATE or QUIT\n";
  }
  return 0;
}
