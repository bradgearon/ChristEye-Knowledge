#include "anaiero/infrastructure/scientific_trace_reader.hpp"
#include "anaiero/services/recorded_electrical_service.hpp"
#include "anaiero/services/recorded_environment_service.hpp"
#include "anaiero/services/coupled_simulation_service.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <string>
#ifdef NDEBUG
#undef NDEBUG
#define assert(e) do{if(!(e))std::abort();}while(false)
#endif
using namespace anaiero;
int main(int argc,char**argv){
  assert(argc==2);
  science::ScientificTrace t{};
  assert(science::load_scientific_trace(argv[1],t)==science::Code::ok);
  assert(t.provider==science::Provider::native_reference&&t.count==112&&
         t.source_epoch==1);
  science::State state{};
  science::RecordedElectricalService electrical(t);
  for(unsigned i=0;i<112;++i)
    assert(electrical.advance(state,{science::Phase::train,320},i%4));
  assert(electrical.remaining()==0);
  assert(!electrical.advance(state,{science::Phase::train,320},0));
  std::ifstream file(argv[1],std::ios::binary);
  std::string content(std::istreambuf_iterator<char>{file},{});
  science::ScientificTrace parsed{};
  content[content.find("provider NATIVE_REFERENCE")+9]='x';
  assert(science::parse_scientific_trace(content,parsed)==science::Code::bad_input);
  const auto environmental=std::filesystem::path(argv[1]).parent_path()/"native-environment.cevscitrace";
  science::ScientificTrace env{};
  assert(science::load_scientific_trace(environmental,env)==science::Code::ok);
  assert(env.channel==science::Channel::environment_proxy&&env.count==28);
  science::RecordedEnvironmentService environment(env);
  science::State environmental_state{};
  for(unsigned i=1;i<=28;++i)
    assert(environment.advance(environmental_state,i));
  assert(environmental_state.environment==60);
  assert(!environment.advance(environmental_state,29));
  std::cout<<"strict sealed source-epoch electrical/environment trace replay PASS\n";
}
