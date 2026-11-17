#pragma once
// Canonical application messages inside the separately bounded TLS framing.
#include "anaiero/contracts/mesh_transport.hpp"
#include "anaiero/contracts/mesh_replay.hpp"
#include "anaiero/contracts/mesh_artifact.hpp"
#include "anaiero/infrastructure/epoch_observation_codec.hpp"
#include <array>
#include <sstream>
#include <span>
#include <string>
#include <string_view>
namespace anaiero::mesh::wire {
enum class Command {publish,subscribe,next,ack,artifact,chunk,ping};
struct Request {
  Command command=Command::ping;
  runtime::Observation observation{};
  std::string id,digest;
  Scope scope{};
  ReplayCursor cursor{};
  std::uint64_t offset=0;
};
inline std::string hex(std::span<const std::byte> bytes){
  static constexpr char digits[]="0123456789abcdef";std::string output(bytes.size()*2,'0');
  for(std::size_t i=0;i<bytes.size();++i){const auto value=std::to_integer<unsigned>(bytes[i]);
    output[i*2]=digits[value>>4];output[i*2+1]=digits[value&15];}
  return output;
}
inline bool unhex(std::string_view text,std::span<std::byte> output)noexcept {
  if(text.size()!=output.size()*2)return false;
  auto value=[](char c)->int{return c>='0'&&c<='9'?c-'0':(c>='a'&&c<='f'?c-'a'+10:-1);};
  for(std::size_t i=0;i<output.size();++i){const auto a=value(text[i*2]),b=value(text[i*2+1]);
    if(a<0||b<0)return false;
    output[i]=static_cast<std::byte>(a*16+b);}
  return true;
}
inline std::string observation_hex(const runtime::Observation& observation){
  std::array<std::byte,runtime::kEpochObservationFrame> bytes{};runtime::EpochObservationCodec codec;
  return codec.encode(observation,bytes)?hex(bytes):std::string{};
}
inline bool parse_observation(std::string_view text,runtime::Observation& observation)noexcept {
  std::array<std::byte,runtime::kEpochObservationFrame> bytes{};runtime::EpochObservationCodec codec;
  return unhex(text,bytes)&&codec.decode(bytes,observation);
}
inline void scope(std::ostream& out,const Scope& value){out<<value.source<<' '<<value.epoch<<' '<<value.clock<<' '<<value.colony;}
inline void cursor(std::ostream& out,const ReplayCursor& value){out<<value.sequence<<' '<<value.hash;}
inline std::string encode(const Request& value){
  std::ostringstream out;out<<"CEVMESH/1 ";
  switch(value.command){
    case Command::publish:{const auto bytes=observation_hex(value.observation);if(bytes.empty())return {};
      out<<"PUBLISH "<<bytes;break;}
    case Command::subscribe:if(!mesh_token(value.id)||!valid_scope(value.scope)||!valid_replay_cursor(value.cursor))return {};
      out<<"SUBSCRIBE "<<value.id<<' ';scope(out,value.scope);out<<' ';cursor(out,value.cursor);break;
    case Command::next:if(!mesh_token(value.id))return {};out<<"NEXT "<<value.id;break;
    case Command::ack:if(!mesh_token(value.id)||!valid_replay_cursor(value.cursor))return {};
      out<<"ACK "<<value.id<<' ';cursor(out,value.cursor);break;
    case Command::artifact:if(!mesh_digest(value.digest))return {};out<<"ARTIFACT "<<value.digest;break;
    case Command::chunk:if(!mesh_digest(value.digest)||value.offset>kMaxMeshArtifactBytes)return {};
      out<<"CHUNK "<<value.digest<<' '<<value.offset;break;
    case Command::ping:out<<"PING";break;
  }
  out<<'\n';return out.str();
}
inline bool decode(std::string_view bytes,Request& output){
  if(bytes.empty()||bytes.size()>kMaxTransportFrameBytes)return false;
  Request value;std::istringstream in{std::string(bytes)};std::string magic,command,text;
  if(!(in>>magic>>command)||magic!="CEVMESH/1")return false;
  if(command=="PUBLISH"){
    value.command=Command::publish;if(!(in>>text)||!parse_observation(text,value.observation))return false;
  }else if(command=="SUBSCRIBE"){
    value.command=Command::subscribe;if(!(in>>value.id>>value.scope.source>>value.scope.epoch>>value.scope.clock>>value.scope.colony>>value.cursor.sequence>>value.cursor.hash))return false;
  }else if(command=="NEXT"){
    value.command=Command::next;if(!(in>>value.id))return false;
  }else if(command=="ACK"){
    value.command=Command::ack;if(!(in>>value.id>>value.cursor.sequence>>value.cursor.hash))return false;
  }else if(command=="ARTIFACT"){
    value.command=Command::artifact;if(!(in>>value.digest))return false;
  }else if(command=="CHUNK"){
    value.command=Command::chunk;if(!(in>>value.digest>>value.offset))return false;
  }else if(command!="PING")return false;
  if((in>>text)||encode(value)!=bytes)return false;
  output=std::move(value);return true;
}
inline std::string error(std::string_view code){return "CEVMESH/1 ERROR "+std::string(code)+'\n';}
inline std::string cursor_reply(std::string_view kind,const ReplayCursor& value){
  std::ostringstream out;out<<"CEVMESH/1 "<<kind<<' ';cursor(out,value);out<<'\n';return out.str();
}
inline bool parse_cursor_reply(std::string_view bytes,std::string_view kind,ReplayCursor& output){
  std::istringstream in{std::string(bytes)};std::string magic,type,extra;ReplayCursor value;
  if(!(in>>magic>>type>>value.sequence>>value.hash)||magic!="CEVMESH/1"||type!=kind||
    (in>>extra)||!valid_replay_cursor(value)||cursor_reply(kind,value)!=bytes)return false;
  output=std::move(value);return true;
}
inline std::string page(const ReplayPage& value){
  if(value.count>kMaxReplayRecords||value.examined>kMaxReplayExamined)return {};
  std::ostringstream out;out<<"CEVMESH/1 PAGE ";cursor(out,value.after);out<<' ';cursor(out,value.terminal);
  out<<' '<<value.count<<' '<<value.examined<<' '<<(value.caught_up?1:0)<<'\n';
  for(std::size_t i=0;i<value.count;++i){cursor(out,value.records[i].cursor);out<<' '<<observation_hex(value.records[i].observation)<<'\n';}
  return out.str();
}
inline bool parse_page(std::string_view bytes,ReplayPage& output){
  if(bytes.size()>kMaxTransportFrameBytes)return false;
  ReplayPage value;std::istringstream in{std::string(bytes)};std::string magic,type,text;unsigned caught=0;
  if(!(in>>magic>>type>>value.after.sequence>>value.after.hash>>value.terminal.sequence>>value.terminal.hash>>value.count>>value.examined>>caught)||
    magic!="CEVMESH/1"||type!="PAGE"||caught>1||value.count>kMaxReplayRecords||value.examined>kMaxReplayExamined||
    value.count>value.examined||!valid_replay_cursor(value.after)||!valid_replay_cursor(value.terminal)||
    value.terminal.sequence<value.after.sequence||value.terminal.sequence-value.after.sequence!=value.examined||
    (value.examined==0&&(value.terminal!=value.after||caught==0)))return false;
  value.caught_up=caught!=0;std::uint64_t previous=value.after.sequence;
  for(std::size_t i=0;i<value.count;++i){auto& record=value.records[i];
    if(!(in>>record.cursor.sequence>>record.cursor.hash>>text)||!valid_replay_cursor(record.cursor)||
      record.cursor.sequence<=previous||record.cursor.sequence>value.terminal.sequence||!parse_observation(text,record.observation))return false;
    if(record.cursor.sequence==value.terminal.sequence&&record.cursor.hash!=value.terminal.hash)return false;
    previous=record.cursor.sequence;
  }
  if((in>>text)||page(value)!=bytes)return false;
  output=std::move(value);return true;
}
inline std::string artifact(const ArtifactReceipt& value){
  return "CEVMESH/1 ARTIFACT "+value.sha256+' '+std::to_string(value.bytes)+'\n';
}
inline bool parse_artifact(std::string_view bytes,ArtifactReceipt& output){
  std::istringstream in{std::string(bytes)};std::string magic,type,extra;ArtifactReceipt value;
  if(!(in>>magic>>type>>value.sha256>>value.bytes)||magic!="CEVMESH/1"||type!="ARTIFACT"||
    (in>>extra)||!mesh_digest(value.sha256)||value.bytes>kMaxMeshArtifactBytes||artifact(value)!=bytes)return false;
  output=std::move(value);return true;
}
inline std::string chunk(const ArtifactChunk& value){
  if(value.count>kMeshArtifactChunkBytes)return {};
  return "CEVMESH/1 CHUNK "+value.artifact.sha256+' '+std::to_string(value.artifact.bytes)+' '+
    std::to_string(value.offset)+' '+std::to_string(value.count)+' '+(value.final?"1":"0")+'\n'+
    hex(std::span<const std::byte>(value.bytes.data(),value.count))+'\n';
}
inline bool parse_chunk(std::string_view bytes,ArtifactChunk& output){
  if(bytes.size()>kMaxTransportFrameBytes)return false;
  std::istringstream in{std::string(bytes)};std::string magic,type,text;ArtifactChunk value;unsigned final=0;
  if(!(in>>magic>>type>>value.artifact.sha256>>value.artifact.bytes>>value.offset>>value.count>>final)||
    magic!="CEVMESH/1"||type!="CHUNK"||final>1||!mesh_digest(value.artifact.sha256)||
    value.artifact.bytes>kMaxMeshArtifactBytes||value.count>kMeshArtifactChunkBytes||
    value.offset>value.artifact.bytes||value.count!=std::min<std::uint64_t>(kMeshArtifactChunkBytes,value.artifact.bytes-value.offset))return false;
  value.final=final!=0;
  if(value.final!=(value.offset+value.count==value.artifact.bytes))return false;
  if(value.count&&(!(in>>text)||!unhex(text,std::span<std::byte>(value.bytes.data(),value.count))))return false;
  if((in>>text)||chunk(value)!=bytes)return false;
  output=std::move(value);return true;
}
} // namespace anaiero::mesh::wire
