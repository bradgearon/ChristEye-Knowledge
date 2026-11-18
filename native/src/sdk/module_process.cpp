#include "process_wire.hpp"
#include "anaiero/infrastructure/native_child_process.hpp"
#include <chrono>

namespace anaiero::sdk {
namespace {
class ProcessProvider;
class ProcessInstance final:public module_detail::Instance {
  std::shared_ptr<ProcessProvider> owner_;anaiero::process::NativeChildProcess process_;
  Bytes creation_;
public:
  explicit ProcessInstance(std::shared_ptr<ProcessProvider> owner):owner_(std::move(owner)){}
  Result start();
  Result initialize(std::string_view);
  Result call(std::string_view,std::string_view,Bytes&)override;
  Result clone(std::unique_ptr<module_detail::Instance>&)override;
  void cancel()noexcept override{process_.cancel();}
};
class ProcessProvider final:public module_detail::Provider,public std::enable_shared_from_this<ProcessProvider> {
public:
  ModuleAdmission admission;
  explicit ProcessProvider(ModuleAdmission value):admission(std::move(value)){}
  Result create(std::string_view input,std::unique_ptr<module_detail::Instance>& output)override {
    auto instance=std::make_unique<ProcessInstance>(shared_from_this());auto result=instance->start();if(!result)return result;
    result=instance->initialize(input);if(!result)return result;
    output=std::move(instance);return {};
  }
};
Result ProcessInstance::start() {
  auto verified=module_detail::verify_artifact(owner_->admission);if(!verified)return verified;
  const auto started=process_.start({owner_->admission.artifact_path,owner_->admission.arguments});
  if(started!=anaiero::process::Code::ok)return {Code::io_error,"supervised module process failed to start"};
  return {};
}
Result ProcessInstance::initialize(std::string_view input) {
  Bytes unused;auto result=call("create",input,unused);if(result)creation_=input;return result;
}
Result ProcessInstance::call(std::string_view command,std::string_view input,Bytes& output) {
  if(!module_detail::command(command)||input.size()>owner_->admission.maximum_wire_bytes)
    return {Code::capacity,"process request wire bound"};
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(owner_->admission.timeout_ms);
  auto exchange=[&](std::string request,std::string& response)->Result {
    const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
    if(remaining<=0){process_.stop();return {Code::deadline,"process module operation deadline exceeded"};}
    const auto result=process_.exchange(request,response,static_cast<std::uint32_t>(remaining));
    if(result==anaiero::process::Code::ok)return {};
    if(result==anaiero::process::Code::cancelled)return {Code::cancelled,"process module operation cancelled"};
    if(result==anaiero::process::Code::deadline)return {Code::deadline,"process module operation deadline exceeded"};
    return {Code::io_error,"process module exchange failed"};
  };
  auto invalid=[&]()->Result {process_.stop();return {Code::corrupt,"process module framing or digest mismatch"};};
  std::string response;
  auto result=exchange("B "+std::string(command)+" "+std::to_string(input.size())+" "+sha256(input),response);if(!result)return result;
  if(response!="A")return invalid();
  for(std::size_t offset=0;offset<input.size();offset+=module_detail::chunk_bytes) {
    result=exchange("W "+std::to_string(offset)+" "+module_detail::hex(input.substr(offset,module_detail::chunk_bytes)),response);
    if(!result)return result;
    if(response!="A")return invalid();
  }
  result=exchange("X",response);if(!result)return result;
  const auto fields=module_detail::words(response);std::uint32_t code=0;std::size_t length=0;
  if(fields.size()!=4||fields[0]!="R"||!module_detail::unsigned_number(fields[1],code)||
      code>static_cast<std::uint32_t>(Code::deadline)||!module_detail::unsigned_number(fields[2],length)||
      length>owner_->admission.maximum_wire_bytes||!valid_digest(fields[3]))return invalid();
  const Digest expected_digest(fields[3]);
  auto remote=module_detail::status(static_cast<std::int32_t>(code),"process module "+std::string(command));if(!remote)return remote;
  Bytes assembled;assembled.reserve(length);
  while(assembled.size()<length) {
    const auto count=std::min(module_detail::chunk_bytes,length-assembled.size());
    result=exchange("R "+std::to_string(assembled.size())+" "+std::to_string(count),response);if(!result)return result;
    Bytes bytes;
    if(!response.starts_with("D ")||!module_detail::unhex(std::string_view(response).substr(2),bytes)||bytes.size()!=count)return invalid();
    assembled+=bytes;
  }
  if(sha256(assembled)!=expected_digest)return invalid();
  output=std::move(assembled);return {};
}
Result ProcessInstance::clone(std::unique_ptr<module_detail::Instance>& output) {
  if(!owner_->descriptor.isolated_trials)return {Code::unsupported,"process module fork capability"};
  Bytes bytes;auto result=call("fork-state",{},bytes);if(!result)return result;
  EngineState state;result=decode_state(bytes,state);if(!result)return result;
  EngineIdentity identity;Envelope configuration;EngineState initial;bool restored=false;
  result=module_detail::decode_creation(creation_,identity,configuration,initial,restored);if(!result)return result;
  return owner_->create(module_detail::creation(identity,state.configuration,&state),output);
}
}
Result load_process_module(const ModuleAdmission& admission,EngineDescriptor& descriptor,std::shared_ptr<IEngineFactory>& output) {
  auto verified=module_detail::verify_artifact(admission);if(!verified)return verified;
  auto owner=std::make_shared<ProcessProvider>(admission);ProcessInstance discovery(owner);
  auto result=discovery.start();if(!result)return result;
  Bytes bytes;result=discovery.call("describe",{},bytes);if(!result)return result;
  result=decode_descriptor(bytes,owner->descriptor);if(!result)return result;
  owner->descriptor.module_sha256=admission.artifact_sha256;
  auto implementation=module_detail::factory(owner);EngineModuleCatalog validation(1);
  result=validation.add(owner->descriptor,implementation);if(!result)return result;
  descriptor=owner->descriptor;output=std::move(implementation);return {};
}
}
