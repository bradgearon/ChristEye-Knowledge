// Native setup utility; no external runtime is needed for the native provider.
#include "anaiero/infrastructure/passive_model_file.hpp"
#include "anaiero/infrastructure/scientific_solver_configuration.hpp"
#include "anaiero/services/passive_membrane_service.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
int main(int argc,char**argv){
  namespace fs=std::filesystem;namespace s=anaiero::science;
  const bool native=argc==5&&std::string_view(argv[1])=="native";
  const bool neuron=argc==6&&std::string_view(argv[1])=="neuron";
  if(!native&&!neuron){
    std::cerr<<"usage: ce_scientific_config native <provider-executable> <model.cevmodel> <output.cevsolver>\n"
      <<"       ce_scientific_config neuron <python> <provider.py> <model.cevmodel> <output.cevsolver>\n";return 2;
  }
  std::error_code error;
  const auto executable=fs::absolute(argv[2],error);if(error)return 2;
  const auto model=fs::absolute(argv[argc-2],error);if(error)return 2;
  const auto output=fs::absolute(argv[argc-1],error);if(error)return 2;
  auto session=output;session.replace_extension(".cevsession");
  if(session==output||fs::exists(output)||fs::exists(session)){std::cerr<<"output already exists or has invalid extension\n";return 2;}
  s::PassiveMembraneModel parsed{};std::string model_hash,script_hash;fs::path script;
  if(!s::files::read_passive_model(model,parsed,model_hash)){std::cerr<<"invalid passive model\n";return 2;}
  if(neuron){
    script=fs::absolute(argv[3],error);std::string bytes;
    if(error||!christeye::living::durable::read_bounded(script,1024*1024,bytes)){std::cerr<<"provider script unavailable\n";return 2;}
    script_hash=christeye::living::plans::sha256_hex(bytes);
  }
  std::ostringstream text;
  text<<"CEVSOLVER/1\nprovider "<<(native?"native-passive-analytic":"neuron-passive-9.0.1")
    <<"\nmodel "<<std::quoted(model.string())<<' '<<model_hash
    <<"\nbehavior "<<christeye::living::plans::sha256_hex(native?s::kPassiveAnalyticBehavior:s::kNeuronPassiveBehavior)
    <<"\nexecutable "<<std::quoted(executable.string())
    <<"\nstep_ns 25000\nmax_steps 1000000\ntimeout_ms "<<(native?2000:10000)
    <<"\ninput_proxy 1000\nunit 2\ncheckpoint_schema 1\narguments "<<(native?1:2)<<'\n';
  if(neuron)text<<"file_argument "<<std::quoted(script.string())<<' '<<script_hash<<'\n';
  text<<"model_argument\nend\n";
  fs::create_directories(output.parent_path(),error);if(error)return 2;
  {std::ofstream file(output,std::ios::binary);file<<text.str();file.close();if(!file){std::cerr<<"cannot write solver configuration\n";return 2;}}
  s::ScientificConfiguration checked;std::string failure;
  if(!s::files::read_solver_configuration(output,checked,failure)){
    fs::remove(output,error);std::cerr<<failure<<'\n';return 2;
  }
  {std::ofstream file(session,std::ios::binary);
    file<<"CEVSESSION/2\nname "<<(native?"native-passive":"neuron-passive")
      <<"\nperiod_ms 100\nstorage segmented 256\nengine 301 scientific 81 1 1 "
      <<std::quoted(output.filename().string())<<" \"-\" 0 0 0 0\nend\n";
    file.close();if(!file){fs::remove(output,error);fs::remove(session,error);std::cerr<<"cannot write session configuration\n";return 2;}
  }
  std::cout<<"solver "<<output.string()<<"\nsession "<<session.string()<<"\nmodel_sha256 "<<model_hash
    <<"\nconfiguration_sha256 "<<checked.configuration_sha256<<'\n';
  return 0;
}
