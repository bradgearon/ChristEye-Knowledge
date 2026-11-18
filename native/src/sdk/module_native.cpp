#include "module_detail.hpp"
#include <cstring>
#include <filesystem>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace anaiero::sdk {
namespace {
class NativeProvider;
class NativeInstance final:public module_detail::Instance {
  std::shared_ptr<NativeProvider> owner_;void* handle_=nullptr;
public:
  NativeInstance(std::shared_ptr<NativeProvider> owner,void* handle):owner_(std::move(owner)),handle_(handle){}
  ~NativeInstance()override;
  Result call(std::string_view,std::string_view,Bytes&)override;
  Result clone(std::unique_ptr<module_detail::Instance>&)override;
  void cancel()noexcept override;
};
class NativeProvider final:public module_detail::Provider,public std::enable_shared_from_this<NativeProvider> {
public:
#ifdef _WIN32
  HMODULE image=nullptr;
#else
  void* image=nullptr;
#endif
  AnaieroModuleApiV1 api{};std::size_t wire_bound=module_detail::maximum_wire;
  ~NativeProvider()override {
    if(image) {
#ifdef _WIN32
      FreeLibrary(image);
#else
      dlclose(image);
#endif
    }
  }
  Result create(std::string_view input,std::unique_ptr<module_detail::Instance>& output)override {
    if(input.size()>wire_bound)return {Code::capacity,"module creation wire bound"};
    void* handle=nullptr;auto result=module_detail::status(api.create(api.context,module_detail::bytes(input),&handle),"module create");
    if(!result){if(handle)api.destroy(api.context,handle);return result;}
    if(!handle)return {Code::corrupt,"module returned null engine"};
    output=std::make_unique<NativeInstance>(shared_from_this(),handle);return {};
  }
};
NativeInstance::~NativeInstance(){if(handle_)owner_->api.destroy(owner_->api.context,handle_);}
Result NativeInstance::call(std::string_view command,std::string_view input,Bytes& output) {
  if(input.size()>owner_->wire_bound)return {Code::capacity,"module request wire bound"};
  auto& api=owner_->api;const auto value=module_detail::bytes(input);std::int32_t result=0;
  if(command=="install"||command=="restore") {
    result=(command=="install"?api.install:api.restore)(api.context,handle_,value);output.clear();
  }else {
    Bytes buffer(owner_->wire_bound,'\0');
    AnaieroBufferV1 target{reinterpret_cast<std::uint8_t*>(buffer.data()),static_cast<std::uint64_t>(buffer.size()),0};
    if(command=="state")result=api.state(api.context,handle_,&target);
    else if(command=="prepare")result=api.prepare(api.context,handle_,value,&target);
    else if(command=="change"&&api.change)result=api.change(api.context,handle_,value,&target);
    else return {Code::unsupported,"module command"};
    auto converted=module_detail::status(result,"module "+std::string(command));if(!converted)return converted;
    if(target.written>target.capacity)return {Code::corrupt,"module exceeded output buffer"};
    buffer.resize(static_cast<std::size_t>(target.written));output=std::move(buffer);
  }
  return module_detail::status(result,"module "+std::string(command));
}
Result NativeInstance::clone(std::unique_ptr<module_detail::Instance>& output) {
  auto& api=owner_->api;if(!api.fork)return {Code::unsupported,"module fork"};
  void* next=nullptr;auto result=module_detail::status(api.fork(api.context,handle_,&next),"module fork");
  if(!result){if(next)api.destroy(api.context,next);return result;}
  if(!next)return {Code::corrupt,"module returned null fork"};
  output=std::make_unique<NativeInstance>(owner_,next);return {};
}
void NativeInstance::cancel()noexcept {if(owner_->api.cancel)owner_->api.cancel(owner_->api.context,handle_);}
}
Result load_native_module(const ModuleAdmission& admission,EngineDescriptor& descriptor,std::shared_ptr<IEngineFactory>& output) {
  if(!admission.trusted_native)return {Code::unauthorized,"in-process native module requires explicit trust admission"};
  auto verified=module_detail::verify_artifact(admission);if(!verified)return verified;
  if(!admission.arguments.empty())return {Code::invalid,"native image has no process arguments"};
  auto owner=std::make_shared<NativeProvider>();owner->wire_bound=admission.maximum_wire_bytes;
  AnaieroModuleEntryV1 entry=nullptr;
#ifdef _WIN32
  const std::filesystem::path path=std::u8string(admission.artifact_path.begin(),admission.artifact_path.end());
  owner->image=LoadLibraryExW(path.c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
  if(!owner->image)return {Code::io_error,"native module load failed"};
  const auto symbol=GetProcAddress(owner->image,"anaiero_module_v1");
#else
  owner->image=dlopen(admission.artifact_path.c_str(),RTLD_NOW|RTLD_LOCAL);
  if(!owner->image)return {Code::io_error,"native module load failed"};
  auto* symbol=dlsym(owner->image,"anaiero_module_v1");
#endif
  static_assert(sizeof(entry)==sizeof(symbol));std::memcpy(&entry,&symbol,sizeof(entry));
  if(!entry)return {Code::unsupported,"native module ABI entry point is missing"};
  const auto* api=entry();
  if(!api||api->abi_version!=ANAIERO_MODULE_ABI_VERSION||api->struct_size<sizeof(AnaieroModuleApiV1)||
      !api->describe||!api->create||!api->destroy||!api->state||!api->prepare||!api->install||!api->restore)
    return {Code::unsupported,"native module ABI version/table mismatch"};
  owner->api=*api;
  Bytes bytes(16384,'\0');AnaieroBufferV1 target{reinterpret_cast<std::uint8_t*>(bytes.data()),static_cast<std::uint64_t>(bytes.size()),0};
  auto described=module_detail::status(api->describe(api->context,&target),"module descriptor");if(!described)return described;
  if(target.written>target.capacity)return {Code::corrupt,"native module descriptor byte bound"};
  bytes.resize(static_cast<std::size_t>(target.written));auto decoded=decode_descriptor(bytes,owner->descriptor);if(!decoded)return decoded;
  owner->descriptor.module_sha256=admission.artifact_sha256;
  if(owner->descriptor.isolated_trials!=(api->fork!=nullptr)||owner->descriptor.change_preparation!=(api->change!=nullptr))
    return {Code::corrupt,"native module advertised capability without matching ABI function"};
  // Detect ordinary replacement during load; native admission assumes the
  // installation location is controlled by the explicitly trusted operator.
  verified=module_detail::verify_artifact(admission);if(!verified)return verified;
  auto implementation=module_detail::factory(owner);EngineModuleCatalog validation(1);
  auto valid=validation.add(owner->descriptor,implementation);if(!valid)return valid;
  descriptor=owner->descriptor;output=std::move(implementation);return {};
}
}
