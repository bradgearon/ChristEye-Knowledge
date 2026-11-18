// Native well-mixed concentration provider. Digital numerical model only.
#include "anaiero/services/environment_compartment_service.hpp"
#include "anaiero/infrastructure/environment_model_file.hpp"
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
static bool line(std::string& text){
  text.clear();
  for(std::size_t i=0;i<16384;++i){
    const int c=std::cin.get();
    if(c==EOF)return false;
    if(c=='\n')return true;
    if(c<32||c>126)return false;
    text+=static_cast<char>(c);
  }
  return false;
}
int main(int argc,char**argv){
  namespace s=anaiero::science;
  if(argc!=2)return 2;
  std::cin.imbue(std::locale::classic());std::cout.imbue(std::locale::classic());
  s::EnvironmentCompartmentModel model{};std::string model_sha;
  if(!s::files::read_environment_model(argv[1],model,model_sha))return 2;
  const auto behavior=christeye::living::plans::sha256_hex(s::kEnvironmentAnalyticBehavior);
  s::EnvironmentCompartmentService service;
  std::uint64_t epoch=0,step=0;bool ready=false;
  auto sample=[&]{
    const auto state=service.state();
    std::cout<<"SAMPLE "<<state.sequence<<' '<<state.sequence*step<<' '
      <<service.concentration_micromolar()<<" 1\n"<<std::flush;
  };
  std::string request;
  while(line(request)){
    std::istringstream input(request);input.imbue(std::locale::classic());
    std::string command,extra;input>>command;
    if(command=="HELLO"){
      unsigned version=0;std::string model_pin,behavior_pin;
      if(ready||!(input>>version>>model_pin>>behavior_pin>>epoch>>step)||input>>extra||version!=1||!epoch||
         model_pin!=model_sha||behavior_pin!=behavior||!service.initialize(model,step))return 3;
      ready=true;
      std::cout<<"READY 1 "<<model_sha<<' '<<behavior<<' '<<epoch<<' '<<step
        <<" 7 1 1000000 native-environment-analytic\n"<<std::flush;
    }else if(!ready)return 3;
    else if(command=="LATEST"){
      if(input>>extra)return 3;
      sample();
    }else if(command=="STEP"){
      std::uint64_t expected_epoch=0,next=0;std::int32_t value=0;
      if(!(input>>expected_epoch>>next>>value)||input>>extra||expected_epoch!=epoch||!service.advance(next,value))return 3;
      sample();
    }else if(command=="CHECKPOINT"){
      if(input>>extra)return 3;
      const auto state=service.state();
      std::cout<<"STATE "<<epoch<<' '<<step<<' '<<state.sequence<<' '
        <<std::setprecision(17)<<state.concentration_micromolar<<'\n'<<std::flush;
    }else if(command=="RESTORE"){
      std::uint64_t restored_epoch=0,restored_step=0;s::EnvironmentCompartmentState state{};
      if(!(input>>restored_epoch>>restored_step>>state.sequence>>state.concentration_micromolar)||input>>extra||
         service.state().sequence!=0||restored_epoch!=epoch||restored_step!=step||!service.restore(state))return 3;
      sample();
    }else return 3;
  }
  return 0;
}
