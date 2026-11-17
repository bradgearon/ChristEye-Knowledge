// Native analytic passive-membrane provider and bounded process fault fixture.
#include "anaiero/services/passive_membrane_service.hpp"
#include "anaiero/infrastructure/passive_model_file.hpp"
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
static bool line(std::string& text){
  text.clear();for(std::size_t i=0;i<16384;++i){const int c=std::cin.get();if(c==EOF)return false;if(c=='\n')return true;if(c<32||c>126)return false;text+=static_cast<char>(c);}return false;
}
int main(int argc,char**argv){
  namespace s=anaiero::science;
  if(argc<2||argc>3)return 2;
  const std::string fault=argc==3?argv[2]:"";
  s::PassiveMembraneModel model{};std::string model_sha;
  if(!s::files::read_passive_model(argv[1],model,model_sha))return 2;
  const auto behavior=christeye::living::plans::sha256_hex(s::kPassiveAnalyticBehavior);
  if(fault=="no-read"){std::this_thread::sleep_for(std::chrono::seconds(120));return 3;}
  s::PassiveMembraneService service;std::uint64_t epoch=0,step=0;bool ready=false;
  auto sample=[&]{const auto state=service.state();std::cout<<"SAMPLE "<<state.sequence<<' '<<state.sequence*step<<' '<<service.voltage_microvolt()<<" 1\n"<<std::flush;};
  std::string request;
  while(line(request)){
    std::istringstream input(request);std::string command,extra;input>>command;
    if(command=="HELLO"){
      unsigned version=0;std::string model_pin,behavior_pin;
      if(ready||!(input>>version>>model_pin>>behavior_pin>>epoch>>step)||input>>extra||version!=1||!epoch||
         model_pin!=model_sha||behavior_pin!=behavior||!service.initialize(model,step))return 3;
      if(fault=="handshake-hang"){std::this_thread::sleep_for(std::chrono::seconds(120));return 3;}
      ready=true;
      std::cout<<"READY 1 "<<model_sha<<' '<<(fault=="wrong-model"?std::string(64,'0'):behavior)<<' '<<epoch<<' '<<step<<" 2 1 1000000 native-passive-analytic\n"<<std::flush;
    }else if(!ready)return 3;
    else if(command=="LATEST"){if(input>>extra)return 3;sample();}
    else if(command=="STEP"){
      if(fault=="hang"){std::this_thread::sleep_for(std::chrono::seconds(120));return 3;}
      if(fault=="crash")std::_Exit(73);
      if(fault=="partial"){std::cout<<"SAMPLE "<<std::flush;std::this_thread::sleep_for(std::chrono::seconds(120));return 3;}
      if(fault=="oversized"){std::cout<<std::string(20000,'x')<<'\n'<<std::flush;return 3;}
      if(fault=="stderr")std::cerr<<std::string(131072,'e')<<std::flush;
      if(fault=="malformed"){std::cout<<"SAMPLE bad\n"<<std::flush;continue;}
      std::uint64_t expected_epoch=0,next=0;int value=0;
      if(!(input>>expected_epoch>>next>>value)||input>>extra||expected_epoch!=epoch||next>1000000||!service.advance(next,value))return 3;
      sample();
    }else if(command=="CHECKPOINT"){
      if(input>>extra)return 3;
      const auto state=service.state();std::cout<<"STATE "<<epoch<<' '<<step<<' '<<state.sequence<<' '<<std::setprecision(17)<<state.voltage_microvolt<<'\n'<<std::flush;
    }else if(command=="RESTORE"){
      std::uint64_t restored_epoch=0,restored_step=0;s::PassiveMembraneState state{};
      if(!(input>>restored_epoch>>restored_step>>state.sequence>>state.voltage_microvolt)||input>>extra||
         service.state().sequence!=0||state.sequence>1000000||restored_epoch!=epoch||restored_step!=step||!service.restore(state))return 3;
      sample();
    }else return 3;
  }
  return 0;
}
