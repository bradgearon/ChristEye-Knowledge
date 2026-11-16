#pragma once
// SIM3.1 single-owner synthetic colony lab; no biological or electrical actuation.
#include "christeye/living/colony_runner.hpp"
#include "christeye/living/durable_registry.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
namespace christeye::living::lab {
namespace sim=christeye::living::sim3;
namespace store=christeye::living::durable;
namespace fs=std::filesystem;
namespace pl=christeye::living::plans;
enum class Code : std::uint8_t {ok, invalid_config, bad_snapshot, store_failure, invalid_measurement};
struct IndexResult {Code code=Code::ok;std::uint32_t indexed=0,duplicates=0;};
inline std::string history_name(std::uint64_t tick) {
  std::ostringstream out;
  out<<"tick-"<<std::setfill('0')<<std::setw(6)<<tick<<".state";
  return out.str();
}
inline std::string json_escape(std::string_view text) {
  std::string out;
  for(char c:text){
    if(c=='\\')out+="\\\\";
    else if(c=='"')out+="\\\"";
    else if(c=='\n')out+="\\n";
    else if(c=='\r')out+="\\r";
    else if(c=='\t')out+="\\t";
    else if(static_cast<unsigned char>(c)>=32)out+=c;
  }
  return out;
}
inline std::string frame(const sim::Configuration& cfg,const sim::Snapshot& state) {
  std::ostringstream out;
  out<<"{\"origin\":\"SIMULATED_UNCALIBRATED\",\"format\":\"CEVLABFRAME/1\",\"run\":\""
     <<json_escape(cfg.name)<<"\",\"tick\":"<<state.tick<<",\"config_sha256\":\""<<cfg.hash
     <<"\",\"snapshot_sha256\":\""<<pl::sha256_hex(sim::encode_snapshot(state))
     <<"\",\"law_version\":"<<cfg.laws.version<<",\"colonies\":[";
  bool first=true;
  for(const auto& c:state.colonies){
    if(!first)out<<',';first=false;
    out<<"{\"id\":\""<<json_escape(c.id)<<"\",\"food\":"<<c.food<<",\"data\":"<<c.data
       <<",\"signal\":"<<c.signal<<",\"stress\":"<<c.stress<<'}';
  }
  out<<"],\"cells\":[";
  first=true;
  for(const auto& c:state.cells){
    if(!first)out<<',';first=false;
    out<<"{\"colony\":\""<<json_escape(c.colony)<<"\",\"id\":"<<c.id<<",\"parent\":"<<c.parent
       <<",\"kind\":\""<<json_escape(c.kind)<<"\",\"x\":"<<c.x<<",\"y\":"<<c.y<<",\"z\":"<<c.z
       <<",\"energy\":"<<c.energy<<",\"maturity\":"<<c.maturity<<",\"learning\":"<<c.learning
       <<",\"birth\":"<<c.birth<<",\"death\":"<<c.death<<",\"alive\":"<<(c.alive?"true":"false")<<'}';
  }
  out<<"],\"links\":[";
  first=true;
  for(const auto& link:cfg.links){
    if(!first)out<<',';first=false;
    out<<"{\"from\":\""<<json_escape(link.from)<<"\",\"to\":\""<<json_escape(link.to)
       <<"\",\"channel\":\""<<sim::channel_text(link.channel)<<"\",\"units\":"<<link.units<<'}';
  }
  out<<"]}";return out.str();
}
inline IndexResult index_history(const sim::Configuration& config,const fs::path& root,
                                 store::Registry& registry,std::uint64_t through_tick) {
  if(!registry.is_open())return {Code::store_failure};
  if(through_tick>10000)return {Code::bad_snapshot};
  IndexResult result{};
  for(std::uint64_t tick=0;tick<=through_tick;++tick){
    std::string bytes;
    sim::Snapshot decoded;
    if(!sim::bounded_read(root/"history"/history_name(tick),sim::kMaxSnapshotBytes,bytes)||
       sim::decode_snapshot(bytes,config,decoded)!=sim::Result::ok || decoded.tick!=tick)
      return {Code::bad_snapshot,result.indexed,result.duplicates};
    auto object=registry.put(bytes);
    if(object.code!=store::Code::ok)return {Code::store_failure,result.indexed,result.duplicates};
    auto previous=registry.event_count();
    auto event=registry.publish("CULTURE_CHECKPOINT",config.name,object.hash,
                                 "SIMULATED",tick,"synthetic-tick","sim3-lab");
    if(event.code!=store::Code::ok)return {Code::store_failure,result.indexed,result.duplicates};
    if(previous==registry.event_count())++result.duplicates;
    else ++result.indexed;
  }
  return result;
}
struct Reference {std::string metric;std::int32_t expected=0,tolerance=0;};
struct Profile {
  std::string id,source_sha256,sha256;
  std::vector<Reference> references;
  static bool read(const fs::path& file,Profile& out) {
    std::string bytes;
    if(!sim::bounded_read(file,65536,bytes))return false;
    std::istringstream in(bytes);
    std::string line;
    if(!std::getline(in,line)||line!="CEVMODEL/1")return false;
    if(!std::getline(in,line)||!line.starts_with("name ")||!sim::ident(line.substr(5)))return false;
    Profile p;p.id=line.substr(5);
    if(!std::getline(in,line)||line!="domain SYNTHETIC_ONLY")return false;
    if(!std::getline(in,line)||!line.starts_with("source_sha256 ")||!sim::digest(line.substr(14)))return false;
    p.source_sha256=line.substr(14);
    bool ended=false;
    while(std::getline(in,line)){
      if(line.empty()||line.front()=='#')continue;
      if(line=="end"){ended=true;break;}
      std::vector<std::string> t;
      if(!sim::tokens(line,t)||t.size()!=4||t[0]!="reference"||
         !(t[1]=="alive"||t[1]=="energy"||t[1]=="learning"||t[1]=="food"||t[1]=="stress")||
         p.references.size()>=32)return false;
      Reference r;r.metric=t[1];
      if(!sim::parse_i32(t[2],r.expected)||!sim::parse_i32(t[3],r.tolerance)||
         r.expected<0||r.tolerance<0)return false;
      for(const auto& prior:p.references)if(prior.metric==r.metric)return false;
      p.references.push_back(r);
    }
    if(!ended||p.references.empty())return false;
    while(std::getline(in,line))if(!line.empty())return false;
    p.sha256=pl::sha256_hex(bytes);
    out=std::move(p);return true;
  }
};
inline std::int64_t measurement(std::string_view key,const sim::Snapshot& state) {
  std::int64_t value=0;
  if(key=="alive"){for(const auto& c:state.cells)if(c.alive)++value;}
  else if(key=="energy"){for(const auto& c:state.cells)value+=c.energy;}
  else if(key=="learning"){for(const auto& c:state.cells)value+=c.learning;}
  else if(key=="food"){for(const auto& c:state.colonies)value+=c.food;}
  else {for(const auto& c:state.colonies)value+=c.stress;}
  return value;
}
inline std::string compare(const Profile& profile,const sim::Configuration& config,
                           const sim::Snapshot& state) {
  std::ostringstream out;
  out<<"{\"classification\":\"SYNTHETIC_MODEL_COMPARISON_NOT_BIOLOGICAL_VALIDATION\",\"profile\":\""
     <<json_escape(profile.id)<<"\",\"profile_sha256\":\""<<profile.sha256
     <<"\",\"tick\":"<<state.tick<<",\"config_sha256\":\""<<config.hash<<"\",\"observations\":[";
  bool first=true;
  for(const auto& reference:profile.references){
    if(!first)out<<',';first=false;
    const auto actual=measurement(reference.metric,state);
    const auto delta=actual-reference.expected;
    const auto pass=delta>=-reference.tolerance && delta<=reference.tolerance;
    out<<"{\"metric\":\""<<reference.metric<<"\",\"expected\":"<<reference.expected
       <<",\"measured\":"<<actual<<",\"tolerance\":"<<reference.tolerance
       <<",\"pass\":"<<(pass?"true":"false")<<'}';
  }
  out<<"]}";return out.str();
}
} // namespace christeye::living::lab
