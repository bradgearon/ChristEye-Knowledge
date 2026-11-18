#pragma once
// CE-SW-12 SIM3: versioned, restartable DIGITAL colony runner.
// Synthetic food, signaling, lineage, and foreign-stress proxies have no
// calibrated biological meaning. No wet-lab or electrical actuation.
#include "christeye/living/durable_io.hpp"
#include "anaiero/models/sim3_state.hpp"
#include "anaiero/services/sim3_transition_service.hpp"
#include "christeye/living/plan_hash.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace christeye::living::sim3 {
namespace fs = std::filesystem;
namespace pl = christeye::living::plans;
namespace os = christeye::living::durable::os;
inline constexpr std::size_t kMaxConfigBytes = 65536;
inline constexpr std::size_t kMaxSourceBytes = 1024 * 1024;
inline constexpr std::size_t kMaxColonies = 8;
inline constexpr std::size_t kMaxCells = 128;
inline constexpr std::size_t kMaxLinks = 48;
inline constexpr std::size_t kMaxProcesses = 64;
inline constexpr std::size_t kMaxSnapshotBytes = 1024 * 1024;

using Result=anaiero::models::sim3::Result;
inline bool ident(std::string_view x) {
  if (x.empty() || x.size() > 56) return false;
  for (char c : x) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
  return true;
}
inline bool digest(std::string_view x) {
  if (x.size() != 64) return false;
  for (char c : x) if (!((c >= 'a' && c <= 'f') || (c >= '0' && c <= '9'))) return false;
  return true;
}
inline bool parse_u32(std::string_view s, std::uint32_t& out) {
  const auto r = std::from_chars(s.data(), s.data()+s.size(), out);
  return r.ec == std::errc{} && r.ptr == s.data()+s.size();
}
inline bool parse_u64(std::string_view s, std::uint64_t& out) {
  const auto r = std::from_chars(s.data(), s.data()+s.size(), out);
  return r.ec == std::errc{} && r.ptr == s.data()+s.size();
}
inline bool parse_i32(std::string_view s, std::int32_t& out) {
  const auto r = std::from_chars(s.data(), s.data()+s.size(), out);
  return r.ec == std::errc{} && r.ptr == s.data()+s.size();
}
inline bool tokens(const std::string& line, std::vector<std::string>& v) {
  if (line.size() > 360) return false;
  std::istringstream in(line);std::string word;
  while (in >> word) { if (word.size()>180 || v.size()>=20) return false;v.push_back(word); }
  return !v.empty();
}
inline bool safe_relative(std::string_view name) {
  if (name.empty() || name.size() > 160 || name.front() == '/') return false;
  const fs::path path(name);
  if (path.has_root_name() || path.has_root_directory()) return false;
  for (const auto& part : path) if (part == "." || part == "..") return false;
  for (char c : name) if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '/' || c == '.' || c == '_' || c == '-')) return false;
  return true;
}
inline bool bounded_read(const fs::path& p, std::size_t max, std::string& out) {
  std::error_code ec;
  if (fs::is_symlink(p,ec) || ec) return false;
  auto size=fs::file_size(p,ec);
  if (ec || size>max) return false;
  std::ifstream file(p,std::ios::binary);
  if(!file)return false;
  out.assign(std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>());
  return !file.bad() && out.size()==size;
}
inline bool bound_path(const fs::path& root, std::string_view rel, fs::path& out) {
  if (!safe_relative(rel)) return false;
  std::error_code ec;
  const auto base=fs::canonical(root,ec);
  if(ec)return false;
  const auto target=fs::canonical(base / std::string(rel),ec);
  if(ec)return false;
  const auto sub=target.lexically_relative(base);
  if(sub.empty() || sub.is_absolute())return false;
  for(const auto& part : sub)if(part=="..")return false;
  out=target;
  return true;
}

using Channel=anaiero::models::sim3::Channel;
inline const char* channel_text(Channel v) noexcept {
  switch(v){case Channel::food:return "food";case Channel::data:return "data";case Channel::stimubus:return "stimubus";}
  return "invalid";
}
inline bool channel_from(std::string_view text,Channel& c){
  for(auto x:{Channel::food,Channel::data,Channel::stimubus})
    if(text==channel_text(x)){c=x;return true;}
  return false;
}
using ProcessKind=anaiero::models::sim3::ProcessKind;
inline const char* process_text(ProcessKind p) noexcept {
  switch(p){case ProcessKind::support:return "support";case ProcessKind::teach:return "teach";
            case ProcessKind::divide:return "divide";case ProcessKind::foreign_stress:return "foreign_stress";}
  return "invalid";
}
inline bool process_from(std::string_view s, ProcessKind& k){
  for(auto p:{ProcessKind::support,ProcessKind::teach,ProcessKind::divide,ProcessKind::foreign_stress})
    if(s==process_text(p)){k=p;return true;}
  return false;
}
// SIM3.1 versioned symbolic laws, not biological rates or intervention instructions.
using ModelLaws=anaiero::models::sim3::ModelLaws;
using ColonyConfig=anaiero::models::sim3::ColonyConfig;
using Seed=anaiero::models::sim3::Seed;
using Link=anaiero::models::sim3::Link;
using Process=anaiero::models::sim3::Process;
struct Configuration:anaiero::models::sim3::ConfigurationData {
  // Parsing is independent of the host filesystem. Admission still verifies
  // every retained source through load() or the modular bundle resolver.
  static Result parse(std::string_view input, Configuration& result) {
    if(input.size()>kMaxConfigBytes)return Result::capacity;
    Configuration candidate;
    std::istringstream in{std::string(input)};std::string line;unsigned section=0;
    bool saw_end=false;
    bool law_version_seen=false;
    std::set<std::string> law_keys;
    while(std::getline(in,line)) {
      if(!line.empty() && line.back()=='\r')line.pop_back();
      if(line.empty() || line.front()=='#')continue;
      std::vector<std::string> t;if(!tokens(line,t))return Result::bad_config;
      if(section==0){if(t.size()!=1 || t[0]!="CEVRUN/1")return Result::bad_config;section=1;continue;}
      if(t[0]=="end") {if(t.size()!=1 || saw_end)return Result::bad_config;saw_end=true;section=2;continue;}
      if(section!=1)return Result::bad_config;
      if(t[0]=="name"&&t.size()==2&&ident(t[1]))candidate.name=t[1];
      else if(t[0]=="law_version"&&t.size()==2&&!law_version_seen&&t[1]=="1") {
        candidate.laws.version=1;law_version_seen=true;
      } else if(t[0]=="law"&&t.size()==3&&law_version_seen) {
        std::uint32_t n=0;
        if(!law_keys.insert(t[1]).second||!parse_u32(t[2],n)||!anaiero::simulation::Sim3TransitionService::set_law(candidate.laws,t[1],n))return Result::bad_config;
      }
      else if(t[0]=="source"&&t.size()==4&&ident(t[1])&&safe_relative(t[2])&&digest(t[3])){
        if(!candidate.material_role.empty()||
           !(t[1]=="synthetic"||t[1]=="blood"||t[1]=="hair"||t[1]=="engineered"))return Result::bad_config;
        candidate.material_role=t[1];candidate.material_path=t[2];candidate.material_sha=t[3];
      }else if(t[0]=="colony"&&t.size()==4&&ident(t[1])&&safe_relative(t[2])&&digest(t[3])){
        candidate.colonies.push_back({t[1],t[2],t[3]});
      }else if(t[0]=="seed"&&t.size()==8&&ident(t[1])&&ident(t[3])){
        Seed s;s.colony=t[1];s.kind=t[3];
        if(!parse_u32(t[2],s.id)||!parse_i32(t[4],s.x)||!parse_i32(t[5],s.y)||
           !parse_i32(t[6],s.z)||!parse_u32(t[7],s.parent))return Result::bad_config;
        candidate.seeds.push_back(s);
      }else if(t[0]=="link"&&t.size()==5){
        Channel type{};std::uint32_t units=0;
        if(!channel_from(t[3],type)||!parse_u32(t[4],units))return Result::bad_config;
        candidate.links.push_back({t[1],t[2],type,units});
      }else if(t[0]=="process"&&t.size()==6){
        ProcessKind type{};std::uint32_t subject=0,value=0,at=0;
        if(!process_from(t[2],type)||!parse_u32(t[3],subject)||
           !parse_u32(t[4],value)||!parse_u32(t[5],at))return Result::bad_config;
        candidate.processes.push_back({t[1],type,subject,value,at});
      }else return Result::bad_config;
      if(candidate.colonies.size()>kMaxColonies || candidate.seeds.size()>kMaxCells ||
         candidate.links.size()>kMaxLinks || candidate.processes.size()>kMaxProcesses)return Result::capacity;
    }
    if(!saw_end || candidate.name.empty()||candidate.material_sha.empty()||candidate.colonies.empty())return Result::bad_config;
    std::set<std::string> names;
    for(const auto& c:candidate.colonies)if(!names.insert(c.id).second)return Result::bad_config;
    std::set<std::string> cell_ids;
    for(const auto& s:candidate.seeds){
      if(!names.contains(s.colony)||s.id==0||s.x < -10000||s.x>10000||s.y < -10000||s.y>10000||s.z < -10000||s.z>10000||
         !(s.kind=="crest_progenitor"||s.kind=="blood_carrier"||s.kind=="neural_progenitor"||s.kind=="neuron"||s.kind=="astrocyte"))return Result::bad_config;
      if(!cell_ids.insert(s.colony+"/"+std::to_string(s.id)).second)return Result::bad_config;
    }
    for(const auto& s:candidate.seeds)if(s.parent && !cell_ids.contains(s.colony+"/"+std::to_string(s.parent)))return Result::bad_config;
    for(const auto& l:candidate.links)if(!names.contains(l.from)||!names.contains(l.to)||l.from==l.to||l.units>100)return Result::bad_config;
    for(const auto& p:candidate.processes){
      if(!names.contains(p.colony)||p.value>100||p.at>10000)return Result::bad_config;
      if((p.kind==ProcessKind::teach||p.kind==ProcessKind::divide) &&
         !cell_ids.contains(p.colony+"/"+std::to_string(p.subject)))return Result::bad_config;
      if(p.kind==ProcessKind::divide && (p.value==0||cell_ids.contains(p.colony+"/"+std::to_string(p.value))))return Result::bad_config;
    }
    candidate.hash=pl::sha256_hex(input);result=std::move(candidate);
    return Result::ok;
  }
  static Result load(const fs::path& file, Configuration& result) {
    std::string input;
    if(!bounded_read(file,kMaxConfigBytes,input))return Result::io_error;
    Configuration candidate;
    if(const auto parsed=parse(input,candidate);parsed!=Result::ok)return parsed;
    const fs::path root=file.parent_path();
    std::vector<std::pair<std::string,std::string>> checks{{candidate.material_path,candidate.material_sha}};
    for(const auto& c:candidate.colonies)checks.emplace_back(c.source_path,c.source_sha);
    for(const auto& [rel,sha]:checks){fs::path path;std::string data;
      if(!bound_path(root,rel,path)||!bounded_read(path,kMaxSourceBytes,data)||pl::sha256_hex(data)!=sha)return Result::bad_hash;
      if(rel==candidate.material_path){
        // Source profiles are provenance-only, never executable genetics.
        if(!data.starts_with("CEVSOURCE/1\n") ||
           data.find("\nsource "+candidate.material_role+"\n")==std::string::npos)
          return Result::bad_config;
      }
    }
    candidate.hash=pl::sha256_hex(input);result=std::move(candidate);
    return Result::ok;
  }
};
using Colony=anaiero::models::sim3::Colony;
using Cell=anaiero::models::sim3::Cell;
using Snapshot=anaiero::models::sim3::Snapshot;
inline std::string encode_snapshot(const Snapshot& state){
  std::ostringstream ss;ss<<"CEVRUNSTATE/1\nconfig "<<state.config_hash<<"\ntick "<<state.tick<<'\n';
  for(const auto& c:state.colonies)ss<<"colony "<<c.id<<' '<<c.food<<' '<<c.data<<' '<<c.signal<<' '<<c.stress<<'\n';
  for(const auto& c:state.cells)ss<<"cell "<<c.colony<<' '<<c.id<<' '<<c.parent<<' '<<c.kind<<' '
      <<c.x<<' '<<c.y<<' '<<c.z<<' '<<c.energy<<' '<<c.maturity<<' '<<c.learning<<' '
      <<c.birth<<' '<<c.death<<' '<<(c.alive?1:0)<<'\n';
  auto raw=ss.str();return raw+"seal "+pl::sha256_hex(raw)+'\n';
}
inline Result decode_snapshot(std::string_view text,const Configuration& config,Snapshot& state){
  if(text.size()>kMaxSnapshotBytes)return Result::capacity;
  std::string_view preseal=text;
  const auto offset=text.rfind("seal ");
  if(offset==text.npos || offset==0 || text[offset-1]!='\n')return Result::corruption;
  auto trailer=text.substr(offset+5);
  if(trailer.size()!=65||trailer.back()!='\n'||pl::sha256_hex(text.substr(0,offset))!=trailer.substr(0,64))return Result::corruption;
  preseal=text.substr(0,offset);
  std::istringstream in(std::string{preseal});std::string line;
  if(!std::getline(in,line)||line!="CEVRUNSTATE/1")return Result::corruption;
  if(!std::getline(in,line)||line!="config "+config.hash)return Result::corruption;
  if(!std::getline(in,line)||!line.starts_with("tick "))return Result::corruption;
  Snapshot tmp;tmp.config_hash=config.hash;
  if(!parse_u64(std::string_view(line).substr(5),tmp.tick))return Result::corruption;
  while(std::getline(in,line)){
    std::vector<std::string> t;if(!tokens(line,t))return Result::corruption;
    if(t[0]=="colony"&&t.size()==6){Colony c;c.id=t[1];
      if(!parse_i32(t[2],c.food)||!parse_i32(t[3],c.data)||!parse_i32(t[4],c.signal)||!parse_i32(t[5],c.stress))return Result::corruption;
      tmp.colonies.push_back(c);
    }else if(t[0]=="cell"&&t.size()==14){Cell c;c.colony=t[1];c.kind=t[4];std::uint32_t active=0;
      if(!parse_u32(t[2],c.id)||!parse_u32(t[3],c.parent)||!parse_i32(t[5],c.x)||!parse_i32(t[6],c.y)||!parse_i32(t[7],c.z)||
        !parse_i32(t[8],c.energy)||!parse_i32(t[9],c.maturity)||!parse_i32(t[10],c.learning)||
        !parse_u64(t[11],c.birth)||!parse_u64(t[12],c.death)||!parse_u32(t[13],active)||active>1)return Result::corruption;
      c.alive=active==1;tmp.cells.push_back(c);
    }else return Result::corruption;
    if(tmp.colonies.size()>kMaxColonies||tmp.cells.size()>kMaxCells)return Result::capacity;
  }
  if(tmp.colonies.size()!=config.colonies.size()||tmp.cells.size()<config.seeds.size())return Result::corruption;
  for(std::size_t i=0;i<config.colonies.size();++i){if(tmp.colonies[i].id!=config.colonies[i].id)return Result::corruption;}
  std::set<std::string> seen;
  for(const auto& c:tmp.cells){
    auto name=c.colony+"/"+std::to_string(c.id);
    if(!seen.insert(name).second||c.id==0||c.energy<0||c.energy>100||c.birth>tmp.tick||
       (c.alive&&c.death!=0)||(!c.alive&&(c.death<c.birth||c.death>tmp.tick)))return Result::corruption;
  }
  for(const auto& c:tmp.cells)if(c.parent&&!seen.contains(c.colony+"/"+std::to_string(c.parent)))return Result::corruption;
  state=std::move(tmp);return Result::ok;
}
inline std::int32_t clamp(std::int64_t v,std::int32_t max=100)noexcept{
  return static_cast<std::int32_t>(std::max<std::int64_t>(0,std::min<std::int64_t>(max,v)));
}
inline std::string escape_xml(std::string_view s){std::string out;for(char c:s){if(c=='&')out+="&amp;";else if(c=='<')out+="&lt;";else if(c=='\"')out+="&quot;";else out+=c;}return out;}
inline std::string svg(const Snapshot& state,const Configuration& config){
  std::ostringstream os;os<<"<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 800 460\">\n"
    <<"<rect width=\"800\" height=\"460\" fill=\"#0d1a2a\"/>\n"
    <<"<text x=\"24\" y=\"32\" fill=\"white\" font-size=\"19\">Anaiero SIM3 / "<<escape_xml(config.name)<<" / tick "<<state.tick<<"</text>\n";
  for(std::size_t ci=0;ci<state.colonies.size();++ci){
    const auto offset=static_cast<int>(ci)*390;
    const auto& colony=state.colonies[ci];
    os<<"<g transform=\"translate("<<offset+20<<",60)\">\n<rect width=\"370\" height=\"370\" rx=\"16\" fill=\"#172b40\" stroke=\"#466d85\"/>\n"
      <<"<text x=\"14\" y=\"26\" fill=\"#e4f7ff\" font-size=\"16\">"<<escape_xml(colony.id)<<" / food "<<colony.food<<" / stress "<<colony.stress<<"</text>\n";
    for(const auto& c:state.cells){
      if(c.colony!=colony.id || !c.parent)continue;
      for(const auto& parent:state.cells)
        if(parent.colony==c.colony && parent.id==c.parent){
          os<<"<line x1=\""<<50+(parent.x%250)<<"\" y1=\""<<55+(parent.y%260)
            <<"\" x2=\""<<50+(c.x%250)<<"\" y2=\""<<55+(c.y%260)
            <<"\" stroke=\"#7391b2\" stroke-width=\"1.5\"/>\n";break;
        }
    }
    for(const auto& c:state.cells){if(c.colony!=colony.id)continue;
      const auto x=50+(c.x%250),y=55+(c.y%260);
      auto color=c.alive?(c.kind=="blood_carrier"?"#ed6679":c.kind=="neuron"?"#74d5ee":"#c3db92"):"#64748b";
      os<<"<circle cx=\""<<x<<"\" cy=\""<<y<<"\" r=\""<<(c.alive?8:5)<<"\" fill=\""<<color<<"\"/>\n"
         <<"<text x=\""<<x+10<<"\" y=\""<<y<<"\" fill=\"#e3e8f4\" font-size=\"10\">"<<c.id<<"</text>\n";
    }
    os<<"</g>\n";
  }
  // Logical inter-colony wiring is displayed separately from lineage.
  for(const auto& edge:config.links){
    std::size_t a=0,b=0;
    for(std::size_t i=0;i<state.colonies.size();++i){
      if(state.colonies[i].id==edge.from)a=i+1;
      if(state.colonies[i].id==edge.to)b=i+1;
    }
    if(a&&b){
      const auto x1=20+static_cast<int>(a-1)*390+185;
      const auto x2=20+static_cast<int>(b-1)*390+185;
      os<<"<path d=\"M"<<x1<<" 440 L"<<x2<<" 440\" stroke=\"#57acc0\" stroke-width=\"2\"/>\n";
    }
  }
  os<<"</svg>\n";return os.str();
}
inline std::string obj(const Snapshot& state){
  std::ostringstream os;os<<"# Anaiero SIM3 synthetic cell and intercolony circuitry model\n";
  std::map<std::string,std::size_t> ids;std::size_t vertex=0;
  for(std::size_t ci=0;ci<state.colonies.size();++ci){const auto& col=state.colonies[ci];
    for(const auto& c:state.cells){if(c.colony!=col.id)continue;
      os<<"g cell_"<<col.id<<'_'<<c.id<<(c.alive?"_active":"_tombstone")<<'\n';
      // Four local vertices provide a visible tetrahedron in generic OBJ viewers.
      const auto x=static_cast<int>(ci)*80+c.x,y=c.y,z=c.z;
      os<<"v "<<x<<' '<<y<<' '<<z+2<<'\n'<<"v "<<x-1<<' '<<y-1<<' '<<z<<'\n'
        <<"v "<<x+1<<' '<<y-1<<' '<<z<<'\n'<<"v "<<x<<' '<<y+1<<' '<<z<<'\n';
      ids[col.id+"/"+std::to_string(c.id)]=vertex+1;
      os<<"f "<<vertex+1<<' '<<vertex+2<<' '<<vertex+3<<'\n';
      os<<"f "<<vertex+1<<' '<<vertex+3<<' '<<vertex+4<<'\n';
      os<<"f "<<vertex+1<<' '<<vertex+4<<' '<<vertex+2<<'\n';
      os<<"f "<<vertex+2<<' '<<vertex+4<<' '<<vertex+3<<'\n';
      vertex+=4;
    }
  }
  // Parent-to-child lineage is drawn as a 3D line; older tombstones remain.
  for(const auto& c:state.cells){
    if(!c.parent)continue;
    auto parent=ids.find(c.colony+"/"+std::to_string(c.parent));
    auto child=ids.find(c.colony+"/"+std::to_string(c.id));
    if(parent!=ids.end() && child!=ids.end())
      os<<"g lineage_"<<c.colony<<"\nl "<<parent->second<<' '<<child->second<<'\n';
  }
  return os.str();
}
inline std::string obj(const Snapshot& state,const Configuration& config) {
  auto model=obj(state);
  std::map<std::string,std::size_t> indices;
  std::size_t count=0;
  for(const auto& colony:state.colonies)
    for(const auto& c:state.cells)
      if(c.colony==colony.id){indices[colony.id+"/"+std::to_string(c.id)]=4*count+1;++count;}
  for(const auto& link:config.links){
    std::size_t a=0,b=0;
    for(const auto& c:state.cells){
      if(c.colony==link.from && a==0)a=indices[c.colony+"/"+std::to_string(c.id)];
      if(c.colony==link.to && b==0)b=indices[c.colony+"/"+std::to_string(c.id)];
    }
    if(a&&b)model+="g circuit_"+link.from+"_"+link.to+"_"+channel_text(link.channel)+
                  "\nl "+std::to_string(a)+" "+std::to_string(b)+"\n";
  }
  return model;
}
class Runner {
  anaiero::simulation::Sim3TransitionService transitions_{};
  Configuration config_{};Snapshot state_{};fs::path store_{};
  os::File lock_{};bool open_=false;
  fs::path history_path(std::uint64_t tick) const {
    std::ostringstream name;name<<"tick-"<<std::setfill('0')<<std::setw(6)<<tick<<".state";
    return store_/"history"/name.str();
  }
  bool retain_snapshot(const Snapshot& snap) const {
    std::error_code ec;
    fs::create_directories(store_/"history",ec);if(ec)return false;
    auto path=history_path(snap.tick);
    auto bytes=encode_snapshot(snap);
    if(fs::exists(path,ec)){
      std::string recorded;
      return !ec && bounded_read(path,kMaxSnapshotBytes,recorded) && recorded==bytes;
    }
    return !ec && os::atomic_write(path,bytes);
  }
  Result apply_tick(Snapshot& next){return transitions_.advance(config_,next);}

public:
  void set_extension(anaiero::contracts::IServiceExtension* extension)noexcept{transitions_.set_extension(extension);}
  Runner()=default;Runner(const Runner&)=delete;Runner&operator=(const Runner&)=delete;
  const Snapshot& state() const noexcept{return state_;}
  const Configuration& configuration() const noexcept{return config_;}
  fs::path committed_history_path() const {return history_path(state_.tick);}
  bool is_open()const noexcept{return open_;}
  Result open(const fs::path& cfg_file,const fs::path& root){
    if(open_)return Result::already_open;
    Configuration cfg;if(auto r=Configuration::load(cfg_file,cfg);r!=Result::ok)return r;
    std::error_code ec;fs::create_directories(root,ec);if(ec)return Result::io_error;
    if(!lock_.open(root/"writer.lock")||!lock_.lock_exclusive())return Result::locked;
    Snapshot candidate;candidate.config_hash=cfg.hash;
    const auto state_path=root/"culture.state";
    if(fs::exists(state_path,ec)){
      std::string data;if(!bounded_read(state_path,kMaxSnapshotBytes,data)||decode_snapshot(data,cfg,candidate)!=Result::ok)return Result::corruption;
    }else{
      for(const auto& c:cfg.colonies)candidate.colonies.push_back({c.id});
      for(const auto& s:cfg.seeds)candidate.cells.push_back({s.colony,s.kind,s.id,s.parent,s.x,s.y,s.z});
      if(!os::atomic_write(state_path,encode_snapshot(candidate)))return Result::io_error;
    }
    config_=std::move(cfg);state_=std::move(candidate);store_=root;
    if(!retain_snapshot(state_))return Result::io_error;
    open_=true;
    return Result::ok;
  }
  Result step(bool save_images=true){
    if(!open_)return Result::invalid_state;
    Snapshot candidate=state_;
    if(auto r=apply_tick(candidate);r!=Result::ok)return r;
    // Preserve every accepted tick (including inactive lineage tombstones)
    // before advancing the durable HEAD-equivalent latest-state pointer.
    // Interrupted writes can leave an unreferenced history object, never a
    // falsely advanced latest-state pointer.
    if(!retain_snapshot(candidate))return Result::uncommitted;
    if(!os::atomic_write(store_/"culture.state",encode_snapshot(candidate)))return Result::uncommitted;
    state_=std::move(candidate); // publish to in-memory readers only AFTER commit
    if(save_images){std::error_code ec;fs::create_directories(store_/"frames",ec);
      if(!ec){
        std::ostringstream filename;filename<<"tick-"<<std::setfill('0')<<std::setw(6)<<state_.tick;
        if(!os::atomic_write(store_/"frames"/(filename.str()+".svg"),svg(state_,config_)) ||
           !os::atomic_write(store_/"frames"/(filename.str()+".obj"),obj(state_,config_)))return Result::io_error;
      } else return Result::io_error;
    }
    return Result::ok;
  }
  std::string snapshot_sha256()const{return pl::sha256_hex(encode_snapshot(state_));}
  bool audit_disk()const{std::string data;Snapshot replay;
    if(!open_||!bounded_read(store_/"culture.state",kMaxSnapshotBytes,data)||
       decode_snapshot(data,config_,replay)!=Result::ok||encode_snapshot(replay)!=encode_snapshot(state_))return false;
    std::string historical;
    return bounded_read(history_path(replay.tick),kMaxSnapshotBytes,historical)&&historical==data;
  }
};
} // namespace christeye::living::sim3
