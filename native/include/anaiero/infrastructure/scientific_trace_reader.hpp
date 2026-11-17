#pragma once
// Strict, sealed offline scientific trace reader. No subprocess launch/loader.
// NEURON and BRAINCELL electrical traces use ACTIVITY_PROXY normalized units;
// PhysiCell/MOOSE/STEPS may supply separately mapped features in future ports.
#include "anaiero/contracts/scientific_trace.hpp"
#include "christeye/living/plan_hash.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
namespace anaiero::science {
inline bool sci_digest(std::string_view s) noexcept {
  if(s.size()!=64)return false;
  for(char c:s)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
  return true;
}
inline bool sci_u32(std::string_view s,std::uint32_t& out) noexcept {
  const auto [ptr,ec]=std::from_chars(s.data(),s.data()+s.size(),out);
  return ec==std::errc{}&&ptr==s.data()+s.size();
}
inline bool sci_provider(std::string_view name,Provider& out) noexcept {
  for(const auto p:{Provider::native_reference,Provider::neuron,Provider::braincell,
                     Provider::physicell,Provider::moose,Provider::steps}) {
    const std::string_view value=p==Provider::native_reference?"NATIVE_REFERENCE":
      p==Provider::neuron?"NEURON":p==Provider::braincell?"BRAINCELL":
      p==Provider::physicell?"PHYSICELL":p==Provider::moose?"MOOSE":"STEPS";
    if(name==value){out=p;return true;}
  }
  return false;
}
inline Code parse_scientific_trace(std::string_view bytes,ScientificTrace& output) {
  if(bytes.size()>32768||bytes.empty()||!bytes.ends_with('\n'))return Code::bad_input;
  const auto pos=bytes.rfind("seal ");
  if(pos==std::string_view::npos||pos==0||bytes[pos-1]!='\n'||
     bytes.size()-pos!=70||!sci_digest(bytes.substr(pos+5,64))||
     christeye::living::plans::sha256_hex(bytes.substr(0,pos))!=bytes.substr(pos+5,64))
    return Code::bad_input;
  std::istringstream input{std::string(bytes.substr(0,pos))};
  std::string line;
  auto next=[&](){return static_cast<bool>(std::getline(input,line))&&!line.empty()&&line.back()!='\r';};
  ScientificTrace data{};
  if(!next()||line!="CEVSCITRACE/1")return Code::bad_input;
  if(!next()||!line.starts_with("provider ")||
     !sci_provider(std::string_view(line).substr(9),data.provider))return Code::bad_input;
  if(!next())return Code::bad_input;
  if(data.provider==Provider::native_reference){
    if(line!="classification SYNTHETIC_FIXTURE")return Code::bad_input;
    data.evidence=Evidence::computational_prototype;
  } else {
    if(line!="classification MODEL_OUTPUT_UNVERIFIED")return Code::bad_input;
    data.evidence=Evidence::published_model_unverified;
  }
  if(!next()||!line.starts_with("channel "))return Code::bad_input;
  const auto channel=std::string_view(line).substr(8);
  if(channel=="ELECTRICAL_ACTIVITY")data.channel=Channel::electrical_activity;
  else if(channel=="ENVIRONMENT_PROXY")data.channel=Channel::environment_proxy;
  else return Code::bad_input;
  if(data.channel==Channel::electrical_activity&&
     data.provider!=Provider::native_reference&&data.provider!=Provider::neuron&&
     data.provider!=Provider::braincell)return Code::bad_input;
  if(data.channel==Channel::environment_proxy&&
     data.provider!=Provider::native_reference&&data.provider!=Provider::physicell&&
     data.provider!=Provider::moose&&data.provider!=Provider::steps)return Code::bad_input;
  if(!next()||!line.starts_with("model_sha256 ")||
     !sci_digest(std::string_view(line).substr(13)))return Code::bad_input;
  data.model_sha256=line.substr(13);
  if(!next()||!line.starts_with("raw_sha256 ")||
     !sci_digest(std::string_view(line).substr(11)))return Code::bad_input;
  data.raw_sha256=line.substr(11);
  if(!next()||!line.starts_with("transform_sha256 ")||
     !sci_digest(std::string_view(line).substr(17)))return Code::bad_input;
  data.transform_sha256=line.substr(17);
  if(!next()||!line.starts_with("source_epoch ")||
     !sci_u32(std::string_view(line).substr(13),data.source_epoch)||
     data.source_epoch==0)return Code::bad_input;
  if(!next()||line!=(data.channel==Channel::electrical_activity?
    "unit DIMENSIONLESS_ACTIVITY_PROXY":"unit DIMENSIONLESS_ENVIRONMENT_PROXY"))
    return Code::bad_input;
  std::uint32_t expected=1;
  bool ended=false;
  while(next()){
    if(line=="end"){ended=true;break;}
    std::istringstream row(line);std::string keyword,step,value,extra;
    if(!(row>>keyword>>step>>value)||(row>>extra)||keyword!="sample")return Code::bad_input;
    std::uint32_t index=0,feature=0;
    if(!sci_u32(step,index)||index!=expected||!sci_u32(value,feature)||
       feature>1000||data.count>=kTraceLimit)return Code::bad_input;
    data.features[data.count++]=static_cast<std::int32_t>(feature);
    ++expected;
  }
  if(!ended||next()||data.count<4)return Code::bad_input;
  data.file_sha256=christeye::living::plans::sha256_hex(bytes);
  output=std::move(data);return Code::ok;
}
inline Code load_scientific_trace(const std::filesystem::path& path,ScientificTrace& out) {
  std::error_code ec;
  if(std::filesystem::is_symlink(path,ec)||ec)return Code::bad_input;
  const auto size=std::filesystem::file_size(path,ec);
  if(ec||size>32768)return Code::bad_input;
  std::ifstream file(path,std::ios::binary);
  if(!file)return Code::bad_input;
  std::string bytes(std::istreambuf_iterator<char>{file},{});
  if(file.bad()||bytes.size()!=size)return Code::bad_input;
  return parse_scientific_trace(bytes,out);
}
} // namespace anaiero::science
