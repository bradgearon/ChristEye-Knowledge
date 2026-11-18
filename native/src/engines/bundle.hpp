#pragma once
// Private, bounded immutable bundle codec. No host path is interpreted here.
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
namespace anaiero::engines::detail {
inline constexpr std::size_t max_bundle_bytes=4*1024*1024;
inline constexpr std::size_t max_asset_bytes=2*1024*1024;
using Bundle=std::map<std::string,std::string>;
inline bool field(std::string_view& input,std::string& output,std::size_t maximum) {
  const auto colon=input.find(':');
  if(colon==std::string_view::npos||colon==0||colon>8)return false;
  std::size_t size=0;
  const auto parsed=std::from_chars(input.data(),input.data()+colon,size);
  if(parsed.ec!=std::errc{}||parsed.ptr!=input.data()+colon||size>maximum||
     input.size()-colon-1<size+1||input[colon+1+size]!=',')return false;
  output.assign(input.substr(colon+1,size));input.remove_prefix(colon+size+2);return true;
}
inline std::string encode(const Bundle& entries) {
  if(entries.size()>128)return {};
  std::string result="CEBUNDLE/1\n";
  for(const auto& [name,bytes]:entries){
    if(name.empty()||name.size()>240||bytes.size()>max_asset_bytes)return {};
    result+=std::to_string(name.size())+":"+name+",";
    result+=std::to_string(bytes.size())+":"+bytes+",";
    if(result.size()>max_bundle_bytes)return {};
  }
  return result;
}
inline bool decode(std::string_view input,Bundle& result) {
  if(input.size()>max_bundle_bytes||!input.starts_with("CEBUNDLE/1\n"))return false;
  input.remove_prefix(11);Bundle parsed;
  while(!input.empty()){
    std::string name,bytes;
    if(parsed.size()>=128||!field(input,name,240)||name.empty()||
       !field(input,bytes,max_asset_bytes)||!parsed.emplace(name,std::move(bytes)).second)return false;
  }
  result=std::move(parsed);return true;
}
inline bool unsigned_number(std::string_view value,std::uint64_t& result) {
  if(value.empty())return false;
  const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
  return parsed.ec==std::errc{}&&parsed.ptr==value.data()+value.size();
}
} // namespace anaiero::engines::detail
