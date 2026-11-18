#pragma once
#include "anaiero/sdk/engines.hpp"
#include "bundle.hpp"
#include <limits>
#include <utility>
namespace anaiero::engines::detail {
using namespace anaiero::sdk;
Result add_cell(EngineModuleCatalog&);
Result add_colony(EngineModuleCatalog&);
Result add_plan(EngineModuleCatalog&);
Result add_numerical(EngineModuleCatalog&);
inline std::string module_id(){return ANAIERO_BUILTIN_BUILD_ID;}
inline Result invalid(std::string message){return {Code::invalid,std::move(message)};}
inline Result corrupt(std::string message){return {Code::corrupt,std::move(message)};}
inline bool has_exact(const Bundle& bundle,std::initializer_list<std::string_view> keys) {
  if(bundle.size()!=keys.size())return false;
  for(const auto key:keys)if(!bundle.contains(std::string(key)))return false;
  return true;
}
inline bool phase_valid(Phase value) noexcept {
  return static_cast<unsigned>(value)<=static_cast<unsigned>(Phase::faulted);
}
inline bool can_run(Phase phase) noexcept {return phase==Phase::ready||phase==Phase::running;}
inline Result guard(const IEngine& engine,const StepRequest& request,bool change=false) {
  if(!valid_token(request.operation_id)||request.expected_revision!=engine.state().revision)
    return {Code::conflict,"operation identity or engine revision mismatch"};
  if(engine.state().revision==std::numeric_limits<std::uint64_t>::max())
    return {Code::capacity,"engine revision exhausted"};
  if(engine.state().phase==Phase::completed)return {Code::completed,"finite engine is completed"};
  if(engine.state().phase==Phase::cancelled)return {Code::cancelled,"engine is cancelled"};
  if(!can_run(engine.state().phase)&&!(change&&engine.state().phase==Phase::paused))
    return {Code::conflict,"engine is not runnable"};
  return {};
}
inline PreparedTransition prepared(const IEngine& engine,const StepRequest& request,EngineState target) {
  PreparedTransition result;
  result.identity=engine.identity();result.operation_id=request.operation_id;
  result.request_sha256=request_digest(engine.identity(),request);
  result.expected_revision=engine.state().revision;
  result.expected_state_sha256=state_digest(engine.state());
  target.revision=engine.state().revision+1;result.target=std::move(target);return result;
}
inline EngineDescriptor descriptor(std::string_view type,std::string name,std::string behavior,
                                   std::string configuration,std::string input,
                                   std::string checkpoint,std::string observation) {
  EngineDescriptor value;
  value.type_id=type;value.display_name=std::move(name);value.behavior_id=std::move(behavior);
  value.module_sha256=module_id();value.configuration_schemas={std::move(configuration)};
  value.input_schemas={std::move(input)};value.checkpoint_schemas={std::move(checkpoint)};
  value.observation_schemas={std::move(observation)};return value;
}
inline bool valid_base(const EngineState& value,std::string_view config_schema,
                        std::string_view checkpoint_schema) {
  return valid_state(value)&&phase_valid(value.phase)&&
    value.configuration.schema==config_schema&&value.checkpoint.schema==checkpoint_schema;
}
}
