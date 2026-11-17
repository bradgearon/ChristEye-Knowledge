#pragma once
// Host-only SHA-256 CAS + committed HEAD. One txn stores World and outbox together.
// No secure network replication or physical actuation. A HEAD publish is the commit point.
#include "anaiero/contracts/fork_optimization.hpp"
#include "anaiero/infrastructure/cell_checkpoint.hpp"
#include "christeye/living/durable_io.hpp"
#include <charconv>
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
namespace anaiero::optimization {
namespace fs=std::filesystem;
namespace sw5=anaiero::cells::files;
namespace os=christeye::living::durable::os;
namespace pl=christeye::living::plans;
inline constexpr std::string_view kZeroHash="0000000000000000000000000000000000000000000000000000000000000000";
inline bool parse_uint(std::string_view s,std::uint64_t& out) noexcept {
  auto [end,error]=std::from_chars(s.data(),s.data()+s.size(),out);
  return error==std::errc{}&&end==s.data()+s.size();
}
inline std::string hex_encode(std::string_view s){
  static constexpr char t[]="0123456789abcdef";
  std::string v;v.reserve(s.size()*2);
  for(char ch:s){const auto c=static_cast<unsigned char>(ch);v+=t[c>>4];v+=t[c&15];}return v;
}
inline bool hex_decode(std::string_view s,std::string& data){
  if(s.size()%2||s.size()>131072)return false;
  auto digit=[](char c)->int{
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return 10+c-'a';
    return -1;};
  data.clear();data.reserve(s.size()/2);
  for(std::size_t i=0;i<s.size();i+=2){
    int a=digit(s[i]),b=digit(s[i+1]);if(a<0||b<0)return false;
    data.push_back(static_cast<char>((a<<4)|b));
  }
  return true;
}
inline std::string ledger_wire(const DigitalRouteLedger& state,const cells::Atlas& atlas) {
  std::ostringstream out;
  out<<"CEVROUTELEDGER/1\n"
     <<"atlas "<<atlas.model_digest<<"\n"
     <<"parent "<<(state.parent_hash.empty()?kZeroHash:std::string_view(state.parent_hash))<<"\n"
     <<"revision "<<state.revision<<"\n"
     <<"observed "<<state.observed_revision<<"\n"
     <<"change "<<state.change_id<<"\n"
     <<"active "<<(state.active?1:0)<<"\n"
     <<"route "<<state.selected.id<<' '<<state.selected.colony<<' '
     <<state.selected.from<<' '<<state.selected.to<<' '
     <<static_cast<unsigned>(state.selected.kind)<<' '
     <<state.selected.digital_cost<<' '<<state.selected.model_risk<<' '
     <<state.selected.guidance_weight<<"\n"
     <<"guide "<<state.applied_guidance<<' '<<(state.prior_existed?1:0)
     <<' '<<state.prior_guidance<<"\n"
     <<"outbox "<<state.event_sequence<<' '
     <<static_cast<unsigned>(state.event_kind)<<' '
     <<(state.event_acked?1:0)<<"\n"
     <<"initial_hex "<<hex_encode(sw5::serialize_world(state.initial_world,atlas))<<"\n"
     <<"current_hex "<<hex_encode(sw5::serialize_world(state.current_world,atlas))<<"\n"
     <<"end\n";
  auto text=out.str();return text+"seal "+pl::sha256_hex(text)+'\n';
}
inline bool ledger_parse(std::string_view bytes,const cells::Atlas& atlas,
                         DigitalRouteLedger& output) {
  if(bytes.empty()||bytes.size()>300000||!bytes.ends_with('\n'))return false;
  auto pos=bytes.rfind("seal ");
  if(pos==bytes.npos||pos==0||bytes[pos-1]!='\n'||bytes.size()-pos!=70||
     pl::sha256_hex(bytes.substr(0,pos))!=bytes.substr(pos+5,64))return false;
  std::istringstream in{std::string(bytes.substr(0,pos))};
  std::string line;
  DigitalRouteLedger s{};s.atlas_hash=atlas.model_digest;
  auto read=[&]()->bool{return bool(std::getline(in,line))&&line.size()<=131090;};
  auto key=[&](std::string_view k,std::string& value)->bool{
    if(!read()||!line.starts_with(std::string(k)+" "))return false;
    value=line.substr(k.size()+1);return true;};
  std::string value;
  auto number=[&](std::string_view k,std::uint64_t& n)->bool{
    return key(k,value)&&parse_uint(value,n);};
  auto limited=[&](std::uint64_t n,std::uint64_t max)->bool{return n<=max;};
  if(!read()||line!="CEVROUTELEDGER/1"||!key("atlas",value)||
     value!=atlas.model_digest||!key("parent",s.parent_hash)||
     !sw5::digest(s.parent_hash)||!number("revision",s.revision)||
     !number("observed",s.observed_revision)||!number("change",s.change_id))return false;
  std::uint64_t active=0;
  if(!number("active",active)||active>1)return false;
  s.active=active!=0;
  if(!key("route",value))return false;
  {
    std::istringstream part(value);std::string t[9];
    for(std::size_t i=0;i<8;++i)if(!(part>>t[i]))return false;
    if(part>>t[8])return false;
    std::uint64_t n[8]{};
    for(std::size_t i=0;i<8;++i)if(!parse_uint(t[i],n[i]))return false;
    if(!limited(n[0],UINT32_MAX)||!limited(n[1],UINT32_MAX)||
       !limited(n[2],UINT32_MAX)||!limited(n[3],UINT32_MAX)||
       n[4]>3||n[5]>1000||n[6]>1000||n[7]>1000)return false;
    s.selected={static_cast<std::uint32_t>(n[0]),static_cast<std::uint32_t>(n[1]),
      static_cast<std::uint32_t>(n[2]),static_cast<std::uint32_t>(n[3]),
      static_cast<RouteKind>(n[4]),static_cast<std::uint16_t>(n[5]),
      static_cast<std::uint16_t>(n[6]),
      static_cast<std::uint16_t>(n[7])};
  }
  if(!key("guide",value))return false;
  {
    std::istringstream part(value);std::string t[4];
    for(std::size_t i=0;i<3;++i)if(!(part>>t[i]))return false;
    if(part>>t[3])return false;
    std::uint64_t n[3]{};
    for(std::size_t i=0;i<3;++i)if(!parse_uint(t[i],n[i]))return false;
    if(n[0]>1000||n[1]>1||n[2]>1000)return false;
    s.applied_guidance=static_cast<std::uint16_t>(n[0]);
    s.prior_existed=n[1]!=0;s.prior_guidance=static_cast<std::uint16_t>(n[2]);
  }
  if(!key("outbox",value))return false;
  {
    std::istringstream part(value);std::string t[4];
    for(std::size_t i=0;i<3;++i)if(!(part>>t[i]))return false;
    if(part>>t[3])return false;
    std::uint64_t n[3]{};
    for(std::size_t i=0;i<3;++i)if(!parse_uint(t[i],n[i]))return false;
    if(n[1]>3||n[2]>1)return false;
    s.event_sequence=n[0];s.event_kind=static_cast<RouteEventKind>(n[1]);
    s.event_acked=n[2]!=0;
  }
  if(!key("initial_hex",value))return false;
  std::string world;
  if(!hex_decode(value,world)||sw5::deserialize_world(world,atlas,s.initial_world)!=cells::Status::ok)return false;
  if(!key("current_hex",value)||!hex_decode(value,world)||
     sw5::deserialize_world(world,atlas,s.current_world)!=cells::Status::ok)return false;
  if(!read()||line!="end"||read()||s.revision>4096||
     (s.revision==0&&s.parent_hash!=kZeroHash)||
     (s.event_sequence==0&&s.event_kind!=RouteEventKind::none)||
     (!s.event_acked&&s.event_kind==RouteEventKind::none)||
     (s.active&&(!s.change_id||!s.selected.id||!s.applied_guidance)))return false;
  output=std::move(s);return true;
}
inline std::string seal_head(std::string_view hash,std::uint64_t revision) {
  auto text="CEVROUTEHEAD/1\nobject "+std::string(hash)+"\nrevision "+
    std::to_string(revision)+"\n";
  return text+"seal "+pl::sha256_hex(text)+'\n';
}
inline bool read_head(std::string_view bytes,std::string& hash,std::uint64_t& revision){
  auto pos=bytes.rfind("seal ");
  if(pos==bytes.npos||pos==0||bytes[pos-1]!='\n'||
     bytes.size()-pos!=70||pl::sha256_hex(bytes.substr(0,pos))!=bytes.substr(pos+5,64))return false;
  std::istringstream in{std::string(bytes.substr(0,pos))};
  std::string line;
  if(!std::getline(in,line)||line!="CEVROUTEHEAD/1"||
     !std::getline(in,line)||!line.starts_with("object "))return false;
  hash=line.substr(7);
  if(!sw5::digest(hash)||!std::getline(in,line)||!line.starts_with("revision ")||
     !parse_uint(std::string_view(line).substr(9),revision)||
     std::getline(in,line))return false;
  return true;
}
class DurableRouteLedger final:public IAtomicRouteLedger {
  fs::path root_{};
  cells::Atlas atlas_{};
  os::File lock_{};
  bool locked_=false;
  bool fail_after_object_=false;
  StoreResult store(const DigitalRouteLedger& value,DigitalRouteLedger& published) noexcept {
    if(!locked_||value.revision>4096)return StoreResult::bad_state;
    const auto bytes=ledger_wire(value,atlas_);
    if(bytes.size()>300000)return StoreResult::capacity;
    const auto hash=pl::sha256_hex(bytes);
    const auto object=root_/"objects"/(hash+".cevopt");
    std::error_code ec;
    if(fs::exists(object,ec)){
      std::string recorded;
      if(!sw5::read_bounded(object,300000,recorded)||recorded!=bytes)
        return StoreResult::corrupt;
    }else if(ec||!os::atomic_write(object,bytes))return StoreResult::io_error;
    if(fail_after_object_){fail_after_object_=false;return StoreResult::io_error;}
    if(!os::atomic_write(root_/"HEAD",seal_head(hash,value.revision)))
      return StoreResult::io_error;
    published=value;published.head_hash=hash;
    published.current_world_sha256=pl::sha256_hex(sw5::serialize_world(value.current_world,atlas_));
    return StoreResult::ok;
  }
public:
  DurableRouteLedger()=default;
  DurableRouteLedger(const DurableRouteLedger&)=delete;
  DurableRouteLedger&operator=(const DurableRouteLedger&)=delete;
  StoreResult open(const fs::path& root,const cells::Atlas& atlas) noexcept {
    if(locked_)return StoreResult::bad_state;
    if(atlas.model_digest.size()!=64||!sw5::digest(atlas.model_digest)||
       !atlas.synthetic_only)return StoreResult::bad_state;
    std::error_code ec;fs::create_directories(root/"objects",ec);
    if(ec||!lock_.open(root/"writer.lock")||!lock_.lock_exclusive())return StoreResult::locked;
    root_=root;atlas_=atlas;locked_=true;return StoreResult::ok;
  }
  void fault_after_object_once() noexcept {fail_after_object_=true;}
  StoreResult read(DigitalRouteLedger& result) noexcept override {
    if(!locked_)return StoreResult::bad_state;
    std::error_code ec;
    if(!fs::exists(root_/"HEAD",ec))return ec?StoreResult::io_error:StoreResult::not_found;
    std::string head,hash;std::uint64_t revision=0;
    if(!sw5::read_bounded(root_/"HEAD",256,head)||!read_head(head,hash,revision))
      return StoreResult::corrupt;
    std::string bytes;
    if(!sw5::read_bounded(root_/"objects"/(hash+".cevopt"),300000,bytes)||
       pl::sha256_hex(bytes)!=hash)return StoreResult::corrupt;
    DigitalRouteLedger candidate{};
    if(!ledger_parse(bytes,atlas_,candidate)||candidate.revision!=revision)return StoreResult::corrupt;
    if(candidate.revision>0){
      std::string parent;
      if(!sw5::read_bounded(root_/"objects"/(candidate.parent_hash+".cevopt"),300000,parent)||
         pl::sha256_hex(parent)!=candidate.parent_hash)return StoreResult::corrupt;
      DigitalRouteLedger predecessor{};
      if(!ledger_parse(parent,atlas_,predecessor)||predecessor.revision+1!=revision)
        return StoreResult::corrupt;
    }
    candidate.head_hash=hash;
    candidate.current_world_sha256=pl::sha256_hex(sw5::serialize_world(candidate.current_world,atlas_));
    result=std::move(candidate);return StoreResult::ok;
  }
  StoreResult initialize(const DigitalRouteLedger& initial,
                         DigitalRouteLedger& written) noexcept override {
    DigitalRouteLedger existing{};
    auto r=read(existing);
    if(r!=StoreResult::not_found)return r==StoreResult::ok?StoreResult::already_exists:r;
    if(initial.revision!=0||initial.active||initial.event_sequence!=0||
       initial.event_kind!=RouteEventKind::none||!initial.event_acked||
       initial.atlas_hash!=atlas_.model_digest)return StoreResult::bad_state;
    DigitalRouteLedger first=initial;first.parent_hash=std::string(kZeroHash);
    return store(first,written);
  }
  StoreResult compare_exchange(const DigitalRouteLedger& prior,
                               DigitalRouteLedger& next) noexcept override {
    DigitalRouteLedger actual{};
    auto r=read(actual);
    if(r!=StoreResult::ok)return r;
    if(actual.revision!=prior.revision||actual.head_hash!=prior.head_hash||
       actual.observed_revision!=prior.observed_revision||
       next.revision!=prior.revision+1||next.atlas_hash!=atlas_.model_digest||
       next.observed_revision!=prior.observed_revision)return StoreResult::conflict;
    next.parent_hash=actual.head_hash;
    return store(next,next);
  }
};
} // namespace anaiero::optimization
