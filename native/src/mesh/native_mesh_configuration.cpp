#include "anaiero/sdk/native_mesh.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <set>
#include <utility>
namespace anaiero::sdk::mesh {
namespace {
Result fail(Code code,std::string why){return {code,std::move(why)};}
bool integer(std::string_view text,std::uint64_t& value){
  if(text.empty())return false;
  const auto result=std::from_chars(text.data(),text.data()+text.size(),value);
  return result.ec==std::errc{}&&result.ptr==text.data()+text.size()&&std::to_string(value)==text;
}
}
Result read_native_server_options(std::string_view path,NativeServerOptions& options){
  if(path.empty()||path.size()>4096||path.find('\0')!=std::string_view::npos)return fail(Code::invalid,"invalid mesh configuration path");
  const std::filesystem::path native_path(std::u8string(reinterpret_cast<const char8_t*>(path.data()),path.size()));
  std::ifstream file(native_path,std::ios::binary);if(!file)return fail(Code::io_error,"cannot open mesh configuration");
  std::string line;if(!std::getline(file,line))return fail(Code::invalid,"empty mesh configuration");
  if(!line.empty()&&line.back()=='\r')line.pop_back();
  if(line!="ANAIEROMESH/2")return fail(Code::invalid,"mesh configuration needs ANAIEROMESH/2");
  std::set<std::string> seen;NativeServerOptions result;std::size_t total=line.size();
  while(std::getline(file,line)){
    total+=line.size();if(total>32768||line.find('\0')!=std::string::npos)return fail(Code::capacity,"mesh configuration exceeds bounds");
    if(!line.empty()&&line.back()=='\r')line.pop_back();
    if(line.empty()||line[0]=='#')continue;
    const auto separator=line.find('=');if(separator==std::string::npos)return fail(Code::invalid,"mesh setting must be key=value");
    const auto key=line.substr(0,separator),text=line.substr(separator+1);if(!seen.insert(key).second)return fail(Code::invalid,"duplicate mesh setting");
    if(key=="certificate")result.credentials.certificate_file=text;
    else if(key=="private_key")result.credentials.private_key_file=text;
    else if(key=="authority")result.credentials.authority_file=text;
    else if(key=="address")result.address=text;
    else {std::uint64_t value=0;if(!integer(text,value))return fail(Code::invalid,"invalid numeric mesh setting");
      if(key=="port"&&value<=65535)result.port=static_cast<std::uint16_t>(value);
      else if(key=="clients"&&value<=32)result.maximum_clients=static_cast<std::size_t>(value);
      else if(key=="handshakes"&&value<=16)result.maximum_handshakes=static_cast<std::size_t>(value);
      else if(key=="handshake_timeout_ms"&&value<=60000)result.handshake_timeout_ms=static_cast<std::uint32_t>(value);
      else if(key=="io_timeout_ms"&&value<=60000)result.io_timeout_ms=static_cast<std::uint32_t>(value);
      else return fail(Code::invalid,"unknown or out-of-range mesh setting");
    }
  }
  if(file.bad())return fail(Code::io_error,"mesh configuration read failed");
  options=std::move(result);return {};
}
}
