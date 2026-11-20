#include "mixed_campaign_codec.hpp"
#include <charconv>
#include <utility>
namespace anaiero::qualification::campaign {
namespace sdk=anaiero::sdk;
bool number(std::string_view bytes,std::uint64_t& value){
  const auto result=std::from_chars(bytes.data(),bytes.data()+bytes.size(),value);
  return !bytes.empty()&&result.ec==std::errc{}&&result.ptr==bytes.data()+bytes.size()&&std::to_string(value)==bytes;
}
sdk::Bytes encode(const State& s){
  return sdk::pack({"ANAIERO-MIXED-CAMPAIGN/1",s.profile,s.session,s.definition,s.build,
    std::to_string(s.attempt),std::to_string(s.serial),std::to_string(s.confirmed_cycles),std::to_string(s.target_cycles),
    std::to_string(s.active_ns),std::to_string(s.attempt_ns),std::to_string(s.recovered_cycles),
    s.active?"1":"0",s.complete?"1":"0",s.pressure?"1":"0",s.control_retry?"1":"0",s.quota?"1":"0",s.restart?"1":"0",s.qualified?"1":"0",s.device});
}
sdk::Result decode(const ev::StoredRecord& retained,State& state){
  std::vector<std::string> f;
  if(retained.record.tombstone||!sdk::unpack(retained.record.payload,f,20,16384)||f.size()!=20||f[0]!="ANAIERO-MIXED-CAMPAIGN/1")
    return {sdk::Code::corrupt,"campaign state is malformed"};
  State s;s.profile=f[1];s.session=f[2];s.definition=f[3];s.build=f[4];s.device=f[19];
  if(!sdk::valid_digest(s.profile)||!sdk::valid_digest(s.session)||!sdk::valid_digest(s.definition)||!sdk::valid_digest(s.build)||(!s.device.empty()&&!sdk::valid_digest(s.device)))
    return {sdk::Code::corrupt,"campaign identity is malformed"};
  std::uint64_t* values[]={&s.attempt,&s.serial,&s.confirmed_cycles,&s.target_cycles,&s.active_ns,&s.attempt_ns,&s.recovered_cycles};
  for(std::size_t i=0;i<7;++i)if(!number(f[i+5],*values[i]))return {sdk::Code::corrupt,"campaign integer is noncanonical"};
  bool* flags[]={&s.active,&s.complete,&s.pressure,&s.control_retry,&s.quota,&s.restart,&s.qualified};
  for(std::size_t i=0;i<7;++i){if(f[i+12]!="0"&&f[i+12]!="1")return {sdk::Code::corrupt,"campaign flag is malformed"};*flags[i]=f[i+12]=="1";}
  if(s.attempt>maximum_attempts||s.serial>maximum_checkpoints||s.confirmed_cycles>s.target_cycles||s.target_cycles>10000||
     s.attempt_ns>s.active_ns||(s.complete&&s.active)||(s.qualified&&(!s.complete||s.attempt!=1))||s.active_ns>maximum_attempts*14ULL*24*60*60*1000000000ULL)
    return {sdk::Code::corrupt,"campaign state exceeds declared bounds"};
  s.retained=retained;state=std::move(s);return {};
}
}
