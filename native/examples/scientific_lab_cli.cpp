// Anaiero's optional scientific simulation foundation. No physical actuation.
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "anaiero/infrastructure/cell_position_port.hpp"
#include "anaiero/infrastructure/scientific_trace_reader.hpp"
#include "anaiero/models/scientific_model_catalog.hpp"
#include "anaiero/services/coupled_simulation_service.hpp"
#include "anaiero/services/recorded_electrical_service.hpp"
#include "anaiero/services/recorded_environment_service.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <string_view>
namespace sci=anaiero::science;
namespace cell=anaiero::cells;
int run_science_bus(const cell::Atlas& atlas);
static const char* condition(sci::Condition c)noexcept{
  switch(c){
    case sci::Condition::neuron_only:return "neuron_only";
    case sci::Condition::astrocyte_coupled:return "astrocyte_coupled";
    case sci::Condition::astrocyte_ablation:return "astrocyte_ablation";
    case sci::Condition::shuffled_glial_signal:return "shuffled_glial_signal";
  }
  return "invalid";
}
static const char* provider(sci::Provider p)noexcept{
  for(const auto& family:sci::kModelFamilies)
    if(family.provider==p)return family.name;
  return "invalid";
}
static bool seed(const cell::Atlas& atlas,cell::ISpatialPort& spatial,cell::World& output){
  cell::CellBehaviorService service(atlas,spatial);
  cell::Cell a{};a.id=3;a.kind=cell::Kind::central_neuron;
  a.position={20,17,2};a.energy=100;
  cell::Cell b=a;b.id=4;b.position={23,17,2};
  cell::Cell c=a;c.id=5;c.kind=cell::Kind::astrocyte;c.position={21,18,2};
  if(service.add(a)!=cell::Status::ok||service.add(b)!=cell::Status::ok||
     service.add(c)!=cell::Status::ok||service.encourage(3,4,700)!=cell::Status::ok)
    return false;
  output=service.world();return true;
}
int main(int argc,char**argv){
  if(argc==2&&std::string_view(argv[1])=="engines"){
    std::cout<<"{\"schema\":\"anaiero.science.engines/1\",\"providers\":[";
    for(std::size_t i=0;i<sci::kModelFamilies.size();++i){
      if(i)std::cout<<',';
      const auto& m=sci::kModelFamilies[i];
      std::cout<<"{\"name\":\""<<m.name<<"\",\"builtin\":"
               <<(m.native_executable_bundled?"true":"false")
               <<",\"validated\":false}";
    }
    std::cout<<"]}\n";return 0;
  }
  if(argc==3&&std::string_view(argv[1])=="trace"){
    sci::ScientificTrace t{};
    if(sci::load_scientific_trace(argv[2],t)!=sci::Code::ok){
      std::cerr<<"invalid / tampered / unsupported trace\n";return 1;
    }
    std::cout<<"{\"schema\":\"anaiero.science.trace/1\",\"provider\":\""
      <<provider(t.provider)<<"\",\"classification\":\""
      <<(t.evidence==sci::Evidence::computational_prototype?
         "COMPUTATIONAL_PROTOTYPE":"MODEL_OUTPUT_UNVERIFIED")
      <<"\",\"channel\":\""
      <<(t.channel==sci::Channel::electrical_activity?"ELECTRICAL_ACTIVITY":"ENVIRONMENT_PROXY")
      <<"\",\"epoch\":"<<t.source_epoch<<",\"samples\":"<<t.count
      <<",\"trace_sha256\":\""<<t.file_sha256<<"\"}\n";
    return 0;
  }
  if(argc<3||argc>4){
    std::cerr<<"usage: anaiero science engines\n"
     <<"       anaiero science trace <file.cevscitrace>\n"
     <<"       anaiero science compare|bus <atlas.cevatlas>\n"
     <<"       anaiero science replay|environment <atlas.cevatlas> <file.cevscitrace>\n";
    return 2;
  }
  const auto action=std::string_view(argv[1]);
  if(action!="compare"&&action!="replay"&&action!="environment"&&action!="bus")return 2;
  if((action=="compare"||action=="bus")&&argc!=3)return 2;
  if((action=="replay"||action=="environment")&&argc!=4)return 2;
  cell::Atlas atlas{};
  if(cell::files::load_checked_atlas(argv[2],atlas)!=cell::Status::ok){
    std::cerr<<"atlas / provenance invalid\n";return 1;
  }
  if(action=="bus")return run_science_bus(atlas);
  cell::BoundedPositionPort spatial;
  cell::World world{};
  if(!seed(atlas,spatial,world))return 1;
  sci::Plan plan{};
  if(action=="compare"){
    std::array<sci::Result,4> results{};
    for(unsigned i=0;i<4;++i){
      sci::ReferenceElectricalService model;
      sci::CoupledSimulationService service(atlas,spatial,model);
      if(service.initialize(world)!=sci::Code::ok)return 1;
      plan.condition=static_cast<sci::Condition>(i+1);
      if(service.run(plan,results[i])!=sci::Code::ok)return 1;
    }
    std::cout<<"{\"schema\":\"anaiero.science.compare/1\","
      <<"\"origin\":\"COMPUTATIONAL_PROTOTYPE\",\"atlas_sha256\":\""
      <<atlas.model_digest<<"\",\"fast_steps_per_tick\":4,\"results\":[";
    for(std::size_t i=0;i<results.size();++i){
      if(i)std::cout<<',';
      std::cout<<"{\"condition\":\""<<condition(results[i].condition)
        <<"\",\"steps\":"<<results[i].steps
        <<",\"retention_proxy\":"<<results[i].end_retention
        <<",\"probe_mean\":"<<results[i].average_probe
        <<",\"viable_proxy_cells\":"<<results[i].viable
        <<",\"candidate_links\":"<<results[i].candidate_links<<"}";
    }
    std::cout<<"]}\n";return 0;
  }
  sci::ScientificTrace t{};
  if(sci::load_scientific_trace(argv[3],t)!=sci::Code::ok)return 1;
  sci::State starting{};starting.epoch=t.source_epoch;
  sci::Result result{};
  plan.condition=sci::Condition::astrocyte_coupled;
  if(action=="replay"){
    if(t.channel!=sci::Channel::electrical_activity||t.count!=112)return 1;
    sci::RecordedElectricalService model(t);
    sci::CoupledSimulationService service(atlas,spatial,model);
    if(service.initialize(world,starting)!=sci::Code::ok||
       service.run(plan,result)!=sci::Code::ok)return 1;
  }else{
    if(t.channel!=sci::Channel::environment_proxy||t.count!=28)return 1;
    sci::RecordedEnvironmentService environment(t);
    sci::ReferenceElectricalService model;
    sci::CoupledSimulationService service(atlas,spatial,model);
    service.set_environment(&environment);
    if(service.initialize(world,starting)!=sci::Code::ok||
       service.run(plan,result)!=sci::Code::ok)return 1;
  }
  std::cout<<"{\"schema\":\"anaiero.science.replay/1\",\"provider\":\""
    <<provider(t.provider)<<"\",\"source_epoch\":"<<t.source_epoch
    <<",\"classification\":\""
    <<(t.evidence==sci::Evidence::computational_prototype?
       "COMPUTATIONAL_PROTOTYPE":"MODEL_OUTPUT_UNVERIFIED")
    <<"\",\"trace_sha256\":\""<<t.file_sha256
    <<"\",\"retention_proxy\":"<<result.end_retention
    <<",\"steps\":"<<result.steps<<"}\n";
}
