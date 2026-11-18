#pragma once
#include "module_detail.hpp"
#include <charconv>

namespace anaiero::sdk::module_detail {
inline constexpr std::size_t chunk_bytes=6000;
inline std::string hex(std::string_view input) {
  constexpr char digits[]="0123456789abcdef";std::string output;output.reserve(input.size()*2);
  for(const auto c:input){const auto value=static_cast<unsigned char>(c);output+=digits[value>>4];output+=digits[value&15];}
  return output;
}
inline bool unhex(std::string_view input,Bytes& output) {
  if(input.size()%2||input.size()>chunk_bytes*2)return false;
  auto digit=[](char c)->int {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;return -1;};
  Bytes result;result.reserve(input.size()/2);
  for(std::size_t i=0;i<input.size();i+=2){const auto hi=digit(input[i]),lo=digit(input[i+1]);if(hi<0||lo<0)return false;result+=static_cast<char>((hi<<4)|lo);}
  output=std::move(result);return true;
}
template<class T> inline bool unsigned_number(std::string_view input,T& output) {
  const auto parsed=std::from_chars(input.data(),input.data()+input.size(),output);
  return !input.empty()&&parsed.ec==std::errc{}&&parsed.ptr==input.data()+input.size()&&std::to_string(output)==input;
}
inline std::vector<std::string_view> words(std::string_view line) {
  std::vector<std::string_view> output;
  while(!line.empty()) {
    const auto next=line.find(' ');output.push_back(line.substr(0,next));
    if(output.size()>8)return {};
    if(next==line.npos)break;
    line.remove_prefix(next+1);
  }
  return output;
}
inline bool command(std::string_view command) {
  return command=="describe"||command=="create"||command=="state"||command=="prepare"||
    command=="install"||command=="restore"||command=="fork-state"||command=="change";
}
}
