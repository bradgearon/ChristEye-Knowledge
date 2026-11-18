#include "module_detail.hpp"
#include "validation.hpp"
#include <filesystem>
#include <fstream>

namespace anaiero::sdk::module_detail {
namespace {
class Proxy final:public IEngine,public ITrialCapability,public IChangeCapability {
  std::shared_ptr<Provider> owner_;std::unique_ptr<Instance> instance_;
  EngineIdentity identity_;EngineState state_;
  Result prepare(const StepRequest& request,PreparedTransition& output,bool change) {
    if(!valid_token(request.operation_id)||request.input.payload.size()>owner_->descriptor.limits.max_payload_bytes||
        !detail::has_schema(change?owner_->descriptor.change_schemas:owner_->descriptor.input_schemas,request.input.schema))
      return {Code::unsupported,"module input schema or byte bound"};
    if(request.expected_revision!=state_.revision)return {Code::conflict,"module source revision changed"};
    Bytes bytes;auto result=instance_->call(change?"change":"prepare",encode_request(request),bytes);if(!result)return result;
    PreparedTransition next;result=decode_transition(bytes,next);if(!result)return result;
    result=detail::validate_transition(identity_,state_,request,next,&owner_->descriptor,change);if(!result)return result;
    Bytes check;result=instance_->call("state",{},check);if(!result)return result;
    EngineState visible;result=decode_state(check,visible);if(!result)return result;
    if(visible!=state_)return {Code::corrupt,"module mutated committed state during prepare"};
    output=std::move(next);return {};
  }
  Result apply(const EngineState& next,bool restoring) {
    if(!valid_state(next,owner_->descriptor.limits)||!detail::has_schema(owner_->descriptor.configuration_schemas,next.configuration.schema))
      return {Code::invalid,"module state/configuration bounds"};
    Bytes unused;auto result=instance_->call(restoring?"restore":"install",encode_state(next),unused);if(!result)return result;
    Bytes check;result=instance_->call("state",{},check);if(!result)return result;
    EngineState actual;result=decode_state(check,actual);if(!result)return result;
    if(actual!=next)return {Code::corrupt,"module install/restore mismatch"};
    state_=next;return {};
  }
public:
  Proxy(std::shared_ptr<Provider> owner,std::unique_ptr<Instance> instance,EngineIdentity identity,EngineState state):
    owner_(std::move(owner)),instance_(std::move(instance)),identity_(std::move(identity)),state_(std::move(state)){}
  const EngineIdentity& identity()const noexcept override{return identity_;}
  const EngineState& state()const noexcept override{return state_;}
  Result prepare_step(const StepRequest& request,PreparedTransition& output)override{return prepare(request,output,false);}
  Result install(const EngineState& state)override{return apply(state,false);}
  Result restore(const EngineState& state)override{return apply(state,true);}
  ITrialCapability* trials()noexcept override{return owner_->descriptor.isolated_trials?this:nullptr;}
  IChangeCapability* changes()noexcept override{return owner_->descriptor.change_preparation?this:nullptr;}
  Result prepare_change(const StepRequest& request,PreparedTransition& output)override {
    return changes()?prepare(request,output,true):Result{Code::unsupported,"module change capability"};
  }
  Result fork(std::unique_ptr<IEngine>& output)const override {
    if(!owner_->descriptor.isolated_trials)return {Code::unsupported,"module trial capability"};
    std::unique_ptr<Instance> isolated;auto result=instance_->clone(isolated);if(!result)return result;
    Bytes bytes;result=isolated->call("state",{},bytes);if(!result)return result;
    EngineState copied;result=decode_state(bytes,copied);if(!result)return result;
    if(copied!=state_)return {Code::corrupt,"module fork state mismatch"};
    output=std::make_unique<Proxy>(owner_,std::move(isolated),identity_,state_);return {};
  }
  void request_cancel()noexcept override{instance_->cancel();}
};
class Factory final:public IEngineFactory {
  std::shared_ptr<Provider> owner_;
public:
  explicit Factory(std::shared_ptr<Provider> owner):owner_(std::move(owner)){}
  Result create(const EngineIdentity& identity,const Envelope& configuration,const EngineState* restored,
      std::unique_ptr<IEngine>& output)override {
    const auto& descriptor=owner_->descriptor;
    if(!valid_identity(identity)||identity.type_id!=descriptor.type_id||identity.module_sha256!=descriptor.module_sha256||
        identity.behavior_id!=descriptor.behavior_id)return {Code::conflict,"module factory identity pin mismatch"};
    if(!detail::has_schema(descriptor.configuration_schemas,configuration.schema)||
        configuration.payload.size()>descriptor.limits.max_payload_bytes||
        (restored&&(!valid_state(*restored,descriptor.limits)||restored->configuration!=configuration)))
      return {Code::invalid,"module factory configuration/checkpoint bounds"};
    std::unique_ptr<Instance> instance;auto result=owner_->create(creation(identity,configuration,restored),instance);if(!result)return result;
    Bytes bytes;result=instance->call("state",{},bytes);if(!result)return result;
    EngineState state;result=decode_state(bytes,state);if(!result)return result;
    if(!valid_state(state,descriptor.limits)||!detail::has_schema(descriptor.checkpoint_schemas,state.checkpoint.schema)||
        state.configuration!=configuration||(restored&&state!=*restored))return {Code::corrupt,"module creation state mismatch"};
    output=std::make_unique<Proxy>(owner_,std::move(instance),identity,std::move(state));return {};
  }
};
}
std::shared_ptr<IEngineFactory> factory(std::shared_ptr<Provider> owner){return std::make_shared<Factory>(std::move(owner));}
Result verify_artifact(const ModuleAdmission& admission) {
  if(admission.artifact_path.empty()||admission.artifact_path.size()>4096||admission.artifact_path.find('\0')!=std::string::npos||
      !valid_digest(admission.artifact_sha256)||!admission.timeout_ms||admission.timeout_ms>60000||
      admission.maximum_wire_bytes<16384||admission.maximum_wire_bytes>maximum_wire||admission.arguments.size()>32)
    return {Code::invalid,"module admission identity or bounds"};
  std::size_t argument_bytes=0;
  for(const auto& argument:admission.arguments) {
    if(argument.size()>4096||argument.find('\0')!=std::string::npos)return {Code::invalid,"module argument"};
    argument_bytes+=argument.size();
  }
  if(argument_bytes>16384)return {Code::capacity,"module argument bytes"};
  std::error_code error;const std::filesystem::path path=std::u8string(admission.artifact_path.begin(),admission.artifact_path.end());
  if(!path.is_absolute()||!std::filesystem::is_regular_file(path,error)||error)return {Code::invalid,"module artifact must be an absolute regular file"};
  const auto size=std::filesystem::file_size(path,error);
  if(error||!size||size>64U*1024U*1024U)return {Code::capacity,"module artifact size"};
  std::ifstream file(path,std::ios::binary);if(!file)return {Code::io_error,"module artifact open"};
  Bytes data(static_cast<std::size_t>(size),'\0');file.read(data.data(),static_cast<std::streamsize>(data.size()));
  if(!file||file.peek()!=std::char_traits<char>::eof())return {Code::io_error,"module artifact changed while reading"};
  if(sha256(data)!=admission.artifact_sha256)return {Code::conflict,"module executable digest mismatch"};
  return {};
}
}
