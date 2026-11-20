#include "process_wire.hpp"
#include <iostream>

namespace anaiero::sdk {
namespace {
struct ProcessEndpoint {
  ModuleApiAdapter adapter;void* engine=nullptr;
  ProcessEndpoint(EngineDescriptor descriptor,std::shared_ptr<IEngineFactory> factory):adapter(std::move(descriptor),std::move(factory)){}
  ~ProcessEndpoint(){if(engine)adapter.api()->destroy(adapter.api()->context,engine);}
  Result execute(std::string_view command,std::string_view input,Bytes& output) {
    const auto& api=*adapter.api();const auto bytes=module_detail::bytes(input);
    if(command=="create") {
      if(engine)return {Code::conflict,"process engine already exists"};
      return module_detail::status(api.create(api.context,bytes,&engine),"create");
    }
    if(command!="describe"&&!engine)return {Code::not_found,"process engine has not been created"};
    if(command=="install"||command=="restore")
      return module_detail::status((command=="install"?api.install:api.restore)(api.context,engine,bytes),command);
    Bytes buffer(module_detail::maximum_wire,'\0');
    AnaieroBufferV1 target{reinterpret_cast<std::uint8_t*>(buffer.data()),static_cast<std::uint64_t>(buffer.size()),0};
    std::int32_t code=0;
    if(command=="describe")code=api.describe(api.context,&target);
    else if(command=="state")code=api.state(api.context,engine,&target);
    else if(command=="prepare")code=api.prepare(api.context,engine,bytes,&target);
    else if(command=="change"&&api.change)code=api.change(api.context,engine,bytes,&target);
    else if(command=="fork-state"&&api.fork) {
      void* isolated=nullptr;code=api.fork(api.context,engine,&isolated);
      if(code==0&&isolated)code=api.state(api.context,isolated,&target);
      else if(code==0)code=static_cast<std::int32_t>(Code::corrupt);
      if(isolated)api.destroy(api.context,isolated);
    }else return {Code::unsupported,"process module command"};
    auto result=module_detail::status(code,command);if(!result)return result;
    if(target.written>target.capacity)return {Code::corrupt,"process module output bound"};
    buffer.resize(static_cast<std::size_t>(target.written));output=std::move(buffer);return {};
  }
};
bool bounded_line(std::string& line) {
  line.clear();char c=0;
  while(std::cin.get(c)) {
    if(c=='\n'){if(!line.empty()&&line.back()=='\r')line.pop_back();return true;}
    if(line.size()==16383)return false;
    line+=c;
  }
  return false;
}
}
int run_module_stdio(EngineDescriptor descriptor,std::shared_ptr<IEngineFactory> factory) {
  ProcessEndpoint endpoint(std::move(descriptor),std::move(factory));
  std::string line,command;Bytes incoming,outgoing;Digest incoming_sha;std::size_t expected=0;bool begun=false,executed=false;
  while(bounded_line(line)) {
    const auto fields=module_detail::words(line);std::string response="E";
    if(fields.size()==4&&fields[0]=="B"&&module_detail::command(fields[1])&&
        module_detail::unsigned_number(fields[2],expected)&&expected<=module_detail::maximum_wire&&valid_digest(fields[3])) {
      command=fields[1];incoming.clear();outgoing.clear();incoming.reserve(expected);incoming_sha=fields[3];begun=true;executed=false;response="A";
    }else if(fields.size()==3&&fields[0]=="W"&&begun&&!executed) {
      std::size_t offset=0;Bytes bytes;
      if(module_detail::unsigned_number(fields[1],offset)&&offset==incoming.size()&&module_detail::unhex(fields[2],bytes)&&
          bytes.size()<=expected-incoming.size()){incoming+=bytes;response="A";}
    }else if(fields.size()==1&&fields[0]=="X"&&begun&&!executed&&incoming.size()==expected&&sha256(incoming)==incoming_sha) {
      const auto result=endpoint.execute(command,incoming,outgoing);executed=true;
      if(!result)outgoing.clear();
      response="R "+std::to_string(static_cast<std::uint32_t>(result.code))+" "+std::to_string(outgoing.size())+" "+sha256(outgoing);
      incoming.clear();
    }else if(fields.size()==3&&fields[0]=="R"&&executed) {
      std::size_t offset=0,length=0;
      if(module_detail::unsigned_number(fields[1],offset)&&module_detail::unsigned_number(fields[2],length)&&
          length>0&&length<=module_detail::chunk_bytes&&offset<=outgoing.size()&&length<=outgoing.size()-offset)
        response="D "+module_detail::hex(std::string_view(outgoing).substr(offset,length));
    }
    std::cout<<response<<'\n'<<std::flush;
    if(!std::cout)return 2;
    if(response=="E")return 3;
  }
  return std::cin.eof()?0:3;
}
}
