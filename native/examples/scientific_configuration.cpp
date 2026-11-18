// Native setup utility; no external runtime is needed for the native provider.
#include "anaiero/infrastructure/passive_model_file.hpp"
#include "anaiero/infrastructure/environment_model_file.hpp"
#include "anaiero/infrastructure/scientific_solver_configuration.hpp"
#include "anaiero/services/passive_membrane_service.hpp"
#include "anaiero/services/environment_compartment_service.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
int main(int argc,char**argv){
  namespace fs=std::filesystem;namespace s=anaiero::science;
  const bool native=argc==5&&std::string_view(argv[1])=="native";
  const bool neuron=argc==6&&std::string_view(argv[1])=="neuron";
  const bool environment=argc==5&&std::string_view(argv[1])=="environment";
  if(!native&&!neuron&&!environment){
    std::cerr<<"usage: ce_scientific_config native <provider-executable> <model.cevmodel> <output.cevsolver>\n"
      <<"       ce_scientific_config neuron <python> <provider.py> <model.cevmodel> <output.cevsolver>\n"
      <<"       ce_scientific_config environment <provider-executable> <model.cevmodel> <output.cevsolver>\n";return 2;
  }
  std::error_code error;
  const auto executable=fs::absolute(argv[2],error);if(error)return 2;
  const auto model=fs::absolute(argv[argc-2],error);if(error)return 2;
  const auto output=fs::absolute(argv[argc-1],error);if(error)return 2;
  auto session=output;session.replace_extension(".cevsession");
  if(session==output||fs::exists(output)||fs::exists(session)){std::cerr<<"output already exists or has invalid extension\n";return 2;}
  std::string model_hash,script_hash;fs::path script;
  if(environment){
    s::EnvironmentCompartmentModel parsed{};
    if(!s::files::read_environment_model(model,parsed,model_hash)||!s::EnvironmentCompartmentService::valid_model(parsed)){
      std::cerr<<"invalid environment model\n";return 2;
    }
  }else{
    s::PassiveMembraneModel parsed{};s::PassiveMembraneService validation;
    if(!s::files::read_passive_model(model,parsed,model_hash)||!validation.initialize(parsed,25000)){
      std::cerr<<"invalid passive model\n";return 2;
    }
  }
  if(neuron){
    script=fs::absolute(argv[3],error);std::string bytes;
    if(error||!christeye::living::durable::read_bounded(script,1024*1024,bytes)){std::cerr<<"provider script unavailable\n";return 2;}
    script_hash=christeye::living::plans::sha256_hex(bytes);
  }
  std::ostringstream text;text.imbue(std::locale::classic());
  const auto provider=environment?"native-environment-analytic":(native?"native-passive-analytic":"neuron-passive-9.0.1");
  const auto behavior=environment?s::kEnvironmentAnalyticBehavior:(native?s::kPassiveAnalyticBehavior:s::kNeuronPassiveBehavior);
  text<<"CEVSOLVER/1\nprovider "<<provider
    <<"\nmodel "<<std::quoted(model.string())<<' '<<model_hash
    <<"\nbehavior "<<christeye::living::plans::sha256_hex(behavior)
    <<"\nexecutable "<<std::quoted(executable.string())
    <<"\nstep_ns "<<(environment?100000000:25000)<<"\nmax_steps 1000000\ntimeout_ms "<<(neuron?10000:2000)
    <<"\ninput_proxy 1000\nunit "<<(environment?7:2)<<"\ncheckpoint_schema 1\narguments "<<(neuron?2:1)<<'\n';
  if(neuron)text<<"file_argument "<<std::quoted(script.string())<<' '<<script_hash<<'\n';
  text<<"model_argument\nend\n";
  fs::create_directories(output.parent_path(),error);if(error)return 2;
  {std::ofstream file(output,std::ios::binary);file<<text.str();file.close();if(!file){std::cerr<<"cannot write solver configuration\n";return 2;}}
  s::ScientificConfiguration checked;std::string failure;
  if(!s::files::read_solver_configuration(output,checked,failure)){
    fs::remove(output,error);std::cerr<<failure<<'\n';return 2;
  }
  {std::ofstream file(session,std::ios::binary);
    file.imbue(std::locale::classic());
    file<<"CEVSESSION/2\nname "<<(environment?"native-environment":(native?"native-passive":"neuron-passive"))
      <<"\nperiod_ms 100\nstorage segmented 256\nengine "<<(environment?302:301)<<" scientific "<<(environment?82:81)<<" 1 1 "
      <<std::quoted(output.filename().string())<<" \"-\" 0 0 0 0\nend\n";
    file.close();if(!file){fs::remove(output,error);fs::remove(session,error);std::cerr<<"cannot write session configuration\n";return 2;}
  }
  std::cout<<"solver "<<output.string()<<"\nsession "<<session.string()<<"\nmodel_sha256 "<<model_hash
    <<"\nconfiguration_sha256 "<<checked.configuration_sha256<<'\n';
  return 0;
}
