#pragma once
#include "anaiero/sdk/module_host.hpp"
#include <algorithm>
#include <exception>

namespace anaiero::sdk::module_detail {
inline constexpr std::size_t maximum_wire=16U*1024U*1024U;
inline AnaieroBytesV1 bytes(std::string_view value) {
  return {reinterpret_cast<const std::uint8_t*>(value.data()),static_cast<std::uint64_t>(value.size())};
}
inline Result status(std::int32_t value,std::string_view action) {
  if(value<0||value>static_cast<std::int32_t>(Code::deadline))return {Code::corrupt,"module returned unknown status"};
  const auto code=static_cast<Code>(value);return {code,code==Code::ok?std::string{}:std::string(action)+": "+std::string(code_name(code))};
}
inline Bytes creation(const EngineIdentity& identity,const Envelope& configuration,const EngineState* restored) {
  return pack({"module-create/1",encode_identity(identity),encode_envelope(configuration),restored?encode_state(*restored):Bytes{}});
}
inline Result decode_creation(std::string_view input,EngineIdentity& identity,Envelope& configuration,
    EngineState& restored,bool& has_restore) {
  std::vector<std::string> fields;auto result=unpack(input,fields,4);if(!result)return result;
  if(fields.size()!=4||fields[0]!="module-create/1"||!decode_identity(fields[1],identity)||
      !decode_envelope(fields[2],configuration))return {Code::corrupt,"module creation payload"};
  has_restore=!fields[3].empty();
  if(has_restore&&!decode_state(fields[3],restored))return {Code::corrupt,"module restored state"};
  return {};
}
struct Instance {
  virtual ~Instance()=default;
  virtual Result call(std::string_view command,std::string_view input,Bytes& output)=0;
  virtual Result clone(std::unique_ptr<Instance>& output)=0;
  virtual void cancel()noexcept=0;
};
struct Provider {
  EngineDescriptor descriptor;
  virtual ~Provider()=default;
  virtual Result create(std::string_view input,std::unique_ptr<Instance>& output)=0;
};
std::shared_ptr<IEngineFactory> factory(std::shared_ptr<Provider>);
Result verify_artifact(const ModuleAdmission&);
}
