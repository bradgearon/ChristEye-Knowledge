#include "common.hpp"
namespace anaiero::engines {
sdk::Result register_builtin_engines(sdk::EngineModuleCatalog& catalog){
  if(auto r=detail::add_cell(catalog);!r)return r;
  if(auto r=detail::add_colony(catalog);!r)return r;
  if(auto r=detail::add_plan(catalog);!r)return r;
  return detail::add_numerical(catalog);
}
sdk::Result identify(const sdk::EngineModuleCatalog& catalog,std::string_view type,
                     std::string_view instance,std::string_view source,std::uint64_t epoch,
                     const sdk::Envelope& config,sdk::EngineIdentity& output){
  const auto* descriptor=catalog.find(type);
  if(!descriptor)return {sdk::Code::not_found,"engine type is not registered"};
  detail::Bundle bundle;if(!detail::decode(config.payload,bundle))return detail::invalid("invalid built-in model bundle");
  sdk::EngineIdentity identity;
  identity.instance_id=instance;identity.source_uid=source;identity.epoch=epoch;identity.type_id=type;
  identity.module_sha256=descriptor->module_sha256;identity.behavior_id=descriptor->behavior_id;
  if(type==vlm_plan)identity.model_sha256=sdk::sha256(config.payload);
  else {
    const auto found=bundle.find(type==cell_behavior?"atlas":"model");
    if(found==bundle.end())return detail::invalid("model bytes are missing from bundle");
    identity.model_sha256=sdk::sha256(found->second);
  }
  if(type==passive_membrane||type==environment){
    const auto step=bundle.find("step_ns");
    if(step==bundle.end()||!detail::unsigned_number(step->second,identity.tick_ns)||!identity.tick_ns)
      return detail::invalid("numerical clock is missing");
    identity.clock_domain="simulated_ns";
  }
  if(!sdk::valid_identity(identity))return detail::invalid("invalid engine or source identity");
  output=std::move(identity);return {};
}
sdk::Envelope no_input(std::string_view type){
  if(type==multi_colony)return {"anaiero.colony-input/1",{}};
  if(type==vlm_plan)return {"anaiero.plan-input/1",{}};
  if(type==passive_membrane||type==environment)return {"anaiero.numerical-input/1",{}};
  if(type==cell_behavior)return cell_input(0,0,0,false);
  return {};
}
}
