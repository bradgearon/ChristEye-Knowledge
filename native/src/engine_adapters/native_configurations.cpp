#include "anaiero/sdk/engine_files.hpp"
#include "anaiero/infrastructure/cell_source_record.hpp"
#include "christeye/living/colony_runner.hpp"
#include "christeye/living/learning_plan.hpp"
#include <filesystem>
#include <iomanip>
#include <map>
#include <sstream>
namespace anaiero::engines {
namespace {
namespace fs=std::filesystem;
namespace pl=christeye::living::plans;
namespace sim=christeye::living::sim3;
using namespace anaiero::sdk;
Result invalid(std::string message){return {Code::invalid,std::move(message)};
}
bool read(const fs::path& path,std::string& bytes,std::size_t limit=4U*1024U*1024U){
  return sim::bounded_read(path,limit,bytes);
}
}
sdk::Result read_cell_configuration(std::string_view atlas_path,std::string_view world_path,sdk::Envelope& output){
  cells::Atlas atlas;
  const fs::path path{atlas_path};
  std::string atlas_bytes,reference,world;
  if(!read(path,atlas_bytes)||cells::files::parse_atlas(atlas_bytes,atlas)!=cells::Status::ok||
     !read(path.parent_path()/atlas.reference_file,reference)||!read(fs::path(world_path),world))
    return invalid("cannot read verified Cell Behavior inputs");
  return cell_configuration(atlas_bytes,reference,world,output);
}
sdk::Result read_colony_configuration(std::string_view model_path,sdk::Envelope& output){
  const fs::path path{model_path};
  sim::Configuration configuration;
  std::string model;
  if(sim::Configuration::load(path,configuration)!=sim::Result::ok||!read(path,model))
    return invalid("cannot read verified Multi-Colony inputs");
  std::map<std::string,std::string> records;
  auto retain=[&](const std::string& name)->bool{
    fs::path source;
    std::string bytes;
    if(!sim::bound_path(path.parent_path(),name,source)||!read(source,bytes))return false;
    records[name]=std::move(bytes);
    return true;
  };
  if(!retain(configuration.material_path))return invalid("missing Multi-Colony source profile");
  for(const auto& colony:configuration.colonies)
    if(!retain(colony.source_path))return invalid("missing Multi-Colony source artifact");
  std::vector<RetainedAsset> assets;
  for(const auto& [name,bytes]:records)assets.push_back({name,bytes});
  return colony_configuration(model,assets,output);
}
sdk::Result read_plan_configuration(std::string_view plan_path,std::string_view base_path,
                                    std::string_view asset_root,sdk::Envelope& output){
  std::string plan_bytes,base;
  pl::Plan plan;
  if(!read(fs::path(plan_path),plan_bytes)||!read(fs::path(base_path),base)||
     pl::Plan::parse(plan_bytes,plan).error!=pl::Error::ok)return invalid("cannot read sealed VLM plan or base");
  std::vector<RetainedAsset> assets;
  for(const auto& asset:plan.artifacts()){
    fs::path path;
    std::string bytes;
    if(!pl::rooted_file(fs::path(asset_root),asset.relative_path,path)||!read(path,bytes,pl::kMaxArtifactBytes))
      return invalid("cannot retain VLM asset inside the declared root");
    assets.push_back({asset.relative_path,std::move(bytes)});
  }
  return plan_configuration(plan_bytes,base,assets,output);
}
sdk::Result read_session_definition(std::string_view configuration_path,
                                    const sdk::EngineModuleCatalog& catalog,sdk::SessionDefinition& output){
  const auto path=fs::absolute(fs::path(configuration_path));
  std::string text;
  if(!read(path,text,64U*1024U))return invalid("cannot read bounded session configuration");
  std::istringstream lines(text);
  std::string line;
  if(!std::getline(lines,line))return invalid("empty session configuration");
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line!="ANAIEROSESSION/1"&&line!="CEVSESSION/1")
    return invalid("session header must be ANAIEROSESSION/1 or legacy CEVSESSION/1");
  const bool legacy=line=="CEVSESSION/1";
  SessionDefinition definition;
  bool ended=false;
  while(std::getline(lines,line)){
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty()||line.front()=='#')continue;
    if(ended)return invalid("data after session end");
    std::istringstream row(line);
    std::string verb,extra;
    row>>verb;
    if(verb=="name"){
      if(!definition.name.empty()||!(row>>definition.name)||row>>extra)return invalid("invalid session name");
    }else if(verb=="period_ms"&&legacy){
      std::uint64_t ignored=0;
      if(!(row>>ignored)||row>>extra||!ignored)return invalid("invalid legacy period");
    }else if(verb=="end"){
      if(row>>extra)return invalid("unexpected end fields");
      ended=true;
    }else if(verb=="engine"){
      EngineSchedule schedule;
      std::string id,type,source,model,initial,assets;
      std::uint64_t epoch=0;
      if(!(row>>id>>type>>source>>epoch>>schedule.cadence)||!schedule.cadence||schedule.cadence>1000)
        return invalid("invalid engine schedule");
      auto resolved=[&](const std::string& name){return (path.parent_path()/fs::path(name)).lexically_normal().string();
      };
      std::string full_type;
      if(type=="sw5"||type=="sim3"){
        std::int32_t support=0,stress=0,signal=0;
        unsigned lesson=0;
        if(!(row>>std::quoted(model)>>std::quoted(initial)>>support>>stress>>signal>>lesson)||row>>extra||lesson>1)
          return invalid("invalid synthetic engine row");
        full_type=type=="sw5"?cell_behavior:multi_colony;
        const auto r=type=="sw5"?read_cell_configuration(resolved(model),resolved(initial),schedule.configuration):
          read_colony_configuration(resolved(model),schedule.configuration);
        if(!r)return r;
        if(type=="sim3"&&(support||stress||signal||lesson))return invalid("Multi-Colony inputs belong in its model configuration");
        schedule.input=type=="sw5"?cell_input(support,stress,signal,lesson==1):no_input(multi_colony);
      }else if(type=="vlm"){
        if(!(row>>std::quoted(model)>>std::quoted(initial)>>std::quoted(assets))||row>>extra)
          return invalid("VLM row requires plan, base and artifact root");
        full_type=vlm_plan;
        if(auto r=read_plan_configuration(resolved(model),resolved(initial),resolved(assets),schedule.configuration);!r)return r;
        schedule.input=no_input(vlm_plan);
      }else if(type=="passive"||type=="environment"){
        std::uint64_t step=0;
        std::int32_t input=0;
        std::string bytes;
        if(!(row>>std::quoted(model)>>step>>input)||row>>extra||!read(resolved(model),bytes,4096))
          return invalid("numerical row requires model, step nanoseconds and input");
        full_type=type=="passive"?passive_membrane:environment;
        if(auto r=numerical_configuration(full_type,bytes,step,input,schedule.configuration);!r)return r;
        schedule.input=no_input(full_type);
      }else return {Code::unsupported,"unrecognized built-in engine alias in file admission"};
      if(auto r=identify(catalog,full_type,id,source,epoch,schedule.configuration,schedule.identity);!r)return r;
      definition.engines.push_back(std::move(schedule));
      if(definition.engines.size()>32)return {Code::capacity,"session admits at most 32 engines"};
    }else return invalid("unknown session configuration field");
  }
  if(!ended)return invalid("missing session end");
  SessionDefinition verified;
  if(auto r=decode_session_definition(encode_session_definition(definition),verified);!r)return r;
  output=std::move(verified);
  return {};
}
}
