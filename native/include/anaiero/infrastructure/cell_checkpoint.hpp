#pragma once
// Immutable-by-seal synthetic cell snapshot; no donor identifiers or genomes.
#include "anaiero/contracts/cell_behavior.hpp"
#include "anaiero/infrastructure/cell_atlas_file.hpp"
#include "christeye/living/durable_io.hpp"
#include <charconv>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
namespace anaiero::cells::files {
inline std::string serialize_world(const World& world,const Atlas& atlas) {
  std::ostringstream s;
  s<<"CEVCELLSTATE/1\natlas "<<atlas.model_digest<<"\ntick "<<world.tick
   <<"\ngeneration "<<world.generation<<"\nplan "
   <<(world.run_plan_digest[0]==0?"none":world.run_plan_digest.data())<<'\n';
  for(std::size_t i=0;i<world.cell_count;++i){
    const auto& c=world.cells[i];
    s<<"cell "<<c.id<<' '<<c.parent_id<<' '<<kind_name(c.kind)<<' '
     <<static_cast<unsigned>(c.stage)<<' '<<c.position.x<<' '<<c.position.y<<' '
     <<c.position.z<<' '<<c.energy<<' '<<c.maturity<<' '<<c.process_proxy<<' '
     <<c.activity_proxy<<' '<<c.learning_proxy<<' '<<c.birth_tick<<' '
     <<c.death_tick<<' '<<(c.viable?1:0)<<" 0\n";
  }
  for(std::size_t i=0;i<world.link_count;++i){
    const auto& l=world.links[i];
    s<<"link "<<l.from<<' '<<l.to<<' '<<l.strength<<' '<<l.established_tick<<'\n';
  }
  for(std::size_t i=0;i<world.intent_count;++i){
    const auto& l=world.intents[i];
    s<<"intent "<<l.from<<' '<<l.to<<' '<<l.guidance_weight<<'\n';
  }
  s<<"end\n";
  const auto preseal=s.str();
  return preseal+"seal "+christeye::living::plans::sha256_hex(preseal)+'\n';
}
inline bool u64(std::string_view s,std::uint64_t& out) noexcept {
  const auto [e,ec]=std::from_chars(s.data(),s.data()+s.size(),out);
  return ec==std::errc{}&&e==s.data()+s.size();
}
inline bool u32(std::string_view s,std::uint32_t& out) noexcept {
  const auto [e,ec]=std::from_chars(s.data(),s.data()+s.size(),out);
  return ec==std::errc{}&&e==s.data()+s.size();
}
inline bool i32(std::string_view s,std::int32_t& out) noexcept {
  const auto [e,ec]=std::from_chars(s.data(),s.data()+s.size(),out);
  return ec==std::errc{}&&e==s.data()+s.size();
}
inline Status deserialize_world(std::string_view bytes,const Atlas& atlas,World& world) {
  if(bytes.size()>65536||!bytes.ends_with('\n'))return Status::corrupt_state;
  const auto start=bytes.rfind("seal ");
  if(start==std::string_view::npos||start==0||bytes[start-1]!='\n'||
     bytes.size()-start!=70)return Status::corrupt_state;
  const auto checksum=bytes.substr(start+5,64);
  if(!digest(checksum)||christeye::living::plans::sha256_hex(bytes.substr(0,start))!=checksum)
    return Status::corrupt_state;
  World draft{};
  std::istringstream in{std::string(bytes.substr(0,start))};
  std::string line;
  auto next=[&](){return bool(std::getline(in,line))&&!line.empty()&&line.back()!='\r';};
  if(!next()||line!="CEVCELLSTATE/1")return Status::corrupt_state;
  if(!next()||line!="atlas "+atlas.model_digest)return Status::bad_profile;
  if(!next()||!line.starts_with("tick ")||!u64(std::string_view(line).substr(5),draft.tick))
    return Status::corrupt_state;
  if(!next()||!line.starts_with("generation ")||
     !u64(std::string_view(line).substr(11),draft.generation))return Status::corrupt_state;
  if(!next()||!line.starts_with("plan "))return Status::corrupt_state;
  if(line!="plan none"){
    auto digest_text=line.substr(5);
    if(!digest(digest_text))return Status::corrupt_state;
    std::copy(digest_text.begin(),digest_text.end(),draft.run_plan_digest.begin());
    draft.run_plan_digest[64]='\0';
  }
  while(next()){
    if(line=="end")break;
    std::vector<std::string> t;
    std::istringstream parts(line);std::string field;
    while(parts>>field){if(t.size()>16)return Status::corrupt_state;t.push_back(field);}
    if(t.empty())return Status::corrupt_state;
    if(t[0]=="cell"&&t.size()==17){
      if(draft.cell_count==kMaxCells)return Status::capacity;
      Cell c{};std::uint32_t stage=0,viable=0;
      if(!u32(t[1],c.id)||!u32(t[2],c.parent_id)||!parse_kind(t[3],c.kind)||
         !u32(t[4],stage)||stage>3||!i32(t[5],c.position.x)||
         !i32(t[6],c.position.y)||!i32(t[7],c.position.z)||
         !i32(t[8],c.energy)||!i32(t[9],c.maturity)||
         !i32(t[10],c.process_proxy)||!i32(t[11],c.activity_proxy)||
         !i32(t[12],c.learning_proxy)||!u64(t[13],c.birth_tick)||
         !u64(t[14],c.death_tick)||!u32(t[15],viable)||viable>1)return Status::corrupt_state;
      // reserved final field establishes explicit compatibility for V2 tags
      if(t[16]!="0")return Status::corrupt_state;
      c.stage=static_cast<Stage>(stage);c.viable=viable==1;
      draft.cells[draft.cell_count++]=c;
    }else if(t[0]=="link"&&t.size()==5){
      if(draft.link_count==kMaxLinks)return Status::capacity;
      Link e{};std::uint32_t weight=0;
      if(!u32(t[1],e.from)||!u32(t[2],e.to)||!u32(t[3],weight)||
         weight>1000||!u64(t[4],e.established_tick))return Status::corrupt_state;
      e.strength=static_cast<std::uint16_t>(weight);draft.links[draft.link_count++]=e;
    }else if(t[0]=="intent"&&t.size()==4){
      if(draft.intent_count==kMaxIntents)return Status::capacity;
      RouteIntent e{};std::uint32_t weight=0;
      if(!u32(t[1],e.from)||!u32(t[2],e.to)||!u32(t[3],weight)||weight>1000)
        return Status::corrupt_state;
      e.guidance_weight=static_cast<std::uint16_t>(weight);
      draft.intents[draft.intent_count++]=e;
    }else return Status::corrupt_state;
  }
  if(line!="end"||next())return Status::corrupt_state;
  const auto status=validate_world_invariants(draft);
  if(status!=Status::ok)return status;
  world=draft;return Status::ok;
}
inline Status read_world(const std::filesystem::path& path,const Atlas& atlas,World& output) {
  std::string bytes;
  if(!read_bounded(path,65536,bytes))return Status::io_failure;
  return deserialize_world(bytes,atlas,output);
}
inline Status write_world(const std::filesystem::path& path,const Atlas& atlas,const World& state) {
  const auto data=serialize_world(state,atlas);
  return christeye::living::durable::os::atomic_write(path,data)
       ?Status::ok:Status::io_failure;
}
} // namespace anaiero::cells::files
