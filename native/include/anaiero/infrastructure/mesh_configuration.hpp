#pragma once
// Canonical local operator files. Their SHA seals detect changes; they do not
// authenticate peers or replace TLS credentials and local enrollment authority.
#include "anaiero/contracts/mesh_identity.hpp"
#include "christeye/living/durable_records.hpp"
#include <charconv>
#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace anaiero::mesh::files {
namespace fs=std::filesystem;
namespace durable=christeye::living::durable;
inline constexpr std::size_t kMaxPolicyBytes=128*1024,kMaxConfigurationBytes=32*1024;
class SystemUnixClock final:public IUnixClock {
public:
  std::uint64_t unix_seconds()const noexcept override {
    const auto value=std::chrono::duration_cast<std::chrono::seconds>(
      std::chrono::system_clock::now().time_since_epoch()).count();
    return value>0?static_cast<std::uint64_t>(value):0;
  }
};
namespace config_detail {
class Lines {
  std::istringstream input_;
public:
  explicit Lines(std::string_view value):input_(std::string(value)){}
  template<class... T> bool row(std::string_view expected,T&&... values){
    std::string line,tag,extra;if(!std::getline(input_,line))return false;
    std::istringstream in(line);if(!(in>>tag)||tag!=expected)return false;
    if constexpr(sizeof...(values)>0)if(!(in>>...>>std::forward<T>(values)))return false;
    return !(in>>extra);
  }
  bool end(){return input_.peek()==std::char_traits<char>::eof();}
};
inline bool ipv4(std::string_view value)noexcept {
  for(unsigned i=0;i<4;++i){const auto dot=value.find('.');
    if((i<3&&dot==std::string_view::npos)||(i==3&&dot!=std::string_view::npos))return false;
    const auto part=value.substr(0,dot);if(part.empty()||part.size()>3||(part.size()>1&&part.front()=='0'))return false;
    unsigned number=0;const auto parsed=std::from_chars(part.data(),part.data()+part.size(),number);
    if(parsed.ec!=std::errc{}||parsed.ptr!=part.data()+part.size()||number>255)return false;
    if(i<3)value.remove_prefix(dot+1);
  }
  return true;
}
inline bool ipv6_parts(std::string_view value,unsigned& groups,bool allow_ipv4)noexcept {
  if(value.empty())return true;
  while(true){const auto colon=value.find(':');const auto part=value.substr(0,colon);
    if(part.empty())return false;
    if(part.find('.')!=std::string_view::npos){
      if(!allow_ipv4||colon!=std::string_view::npos||!ipv4(part))return false;
      groups+=2;
    }else {
      if(part.size()>4)return false;
      for(const char c:part)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;
      ++groups;
    }
    if(groups>8)return false;
    if(colon==std::string_view::npos)return true;
    value.remove_prefix(colon+1);
  }
}
inline bool numeric_address(std::string_view value)noexcept {
  if(value.empty()||value.size()>45)return false;
  if(value.find(':')==std::string_view::npos)return ipv4(value);
  const auto compressed=value.find("::");unsigned groups=0;
  if(compressed==std::string_view::npos)return ipv6_parts(value,groups,true)&&groups==8;
  if(value.find("::",compressed+2)!=std::string_view::npos)return false;
  return ipv6_parts(value.substr(0,compressed),groups,false)&&
    ipv6_parts(value.substr(compressed+2),groups,true)&&groups<8;
}
inline bool local_path(std::string_view value)noexcept {
  if(value.empty()||value.size()>4096||value.find("://")!=std::string_view::npos||
     value.starts_with("//")||value.starts_with("\\\\"))return false;
  for(const char raw:value){const auto c=static_cast<unsigned char>(raw);if(c<32||c==127)return false;}
  return true;
}
} // namespace config_detail

struct MeshConfiguration {
  std::string name,principal,certificate_file,private_key_file,ca_file;
  std::string listen_address="127.0.0.1",connect_address="127.0.0.1",server_pin;
  std::uint16_t listen_port=0,connect_port=0;
  std::uint32_t deadline_ms=5000;
  std::array<Scope,kMaxMeshScopes> local_sources{};
  std::size_t local_source_count=0;
  // Derived local paths; never serialized or used as a source of authority.
  fs::path certificate_path,private_key_path,ca_path;
  std::string configuration_sha256;
};
inline bool valid_configuration(const MeshConfiguration& value)noexcept {
  if(!mesh_token(value.name)||!mesh_token(value.principal)||
    !config_detail::local_path(value.certificate_file)||!config_detail::local_path(value.private_key_file)||
    !config_detail::local_path(value.ca_file)||!config_detail::numeric_address(value.listen_address)||
    !config_detail::numeric_address(value.connect_address)||(!value.listen_port&&!value.connect_port)||
    !value.deadline_ms||value.deadline_ms>60000||value.local_source_count>kMaxMeshScopes||
    (value.connect_port?!mesh_digest(value.server_pin):(!value.server_pin.empty()&&!mesh_digest(value.server_pin))))return false;
  for(std::size_t i=0;i<value.local_sources.size();++i){
    if(i>=value.local_source_count){if(value.local_sources[i]!=Scope{})return false;continue;}
    if(!valid_scope(value.local_sources[i]))return false;
    for(std::size_t j=0;j<i;++j)if(value.local_sources[i].source==value.local_sources[j].source&&
      value.local_sources[i].epoch==value.local_sources[j].epoch)return false;
  }
  return true;
}
inline std::string serialize_configuration(const MeshConfiguration& value){
  if(!valid_configuration(value))return {};
  std::ostringstream out;out<<"CEVMESH/1\nname "<<value.name<<"\nprincipal "<<value.principal
    <<"\ncertificate "<<std::quoted(value.certificate_file)<<"\nprivate_key "<<std::quoted(value.private_key_file)
    <<"\nca "<<std::quoted(value.ca_file)<<"\nlisten "<<value.listen_address<<' '<<value.listen_port
    <<"\nconnect "<<value.connect_address<<' '<<value.connect_port<<"\nserver_pin "<<(value.server_pin.empty()?"none":value.server_pin)
    <<"\ndeadline_ms "<<value.deadline_ms<<"\nsources "<<value.local_source_count<<'\n';
  for(std::size_t i=0;i<value.local_source_count;++i){const auto& s=value.local_sources[i];
    out<<"source "<<s.source<<' '<<s.epoch<<' '<<s.clock<<' '<<s.colony<<'\n';}
  const auto body=out.str();out<<"seal "<<durable::pl::sha256_hex(body)<<'\n';return out.str();
}
inline bool parse_configuration(std::string_view bytes,MeshConfiguration& output,std::string& error){
  error.clear();if(bytes.size()>kMaxConfigurationBytes){error="mesh configuration exceeds bound";return false;}
  MeshConfiguration next;config_detail::Lines in(bytes);std::string seal;unsigned listen=0,connect=0;
  if(!in.row("CEVMESH/1")||!in.row("name",next.name)||!in.row("principal",next.principal)||
    !in.row("certificate",std::quoted(next.certificate_file))||!in.row("private_key",std::quoted(next.private_key_file))||
    !in.row("ca",std::quoted(next.ca_file))||!in.row("listen",next.listen_address,listen)||
    !in.row("connect",next.connect_address,connect)||listen>65535||connect>65535||
    !in.row("server_pin",next.server_pin)||!in.row("deadline_ms",next.deadline_ms)||
    !in.row("sources",next.local_source_count)||next.local_source_count>kMaxMeshScopes){error="invalid mesh configuration fields";return false;}
  next.listen_port=static_cast<std::uint16_t>(listen);next.connect_port=static_cast<std::uint16_t>(connect);
  if(next.server_pin=="none")next.server_pin.clear();
  for(std::size_t i=0;i<next.local_source_count;++i){auto& s=next.local_sources[i];
    if(!in.row("source",s.source,s.epoch,s.clock,s.colony)){error="invalid local source scope";return false;}}
  if(!in.row("seal",seal)||!in.end()||!mesh_digest(seal)||!valid_configuration(next)||serialize_configuration(next)!=bytes){
    error="invalid mesh configuration seal or canonical encoding";return false;}
  next.configuration_sha256=durable::pl::sha256_hex(bytes);output=std::move(next);return true;
}
inline bool load_configuration(const fs::path& path,MeshConfiguration& output,std::string& error){
  std::string bytes;if(!durable::read_bounded(path,kMaxConfigurationBytes,bytes)){error="cannot read local mesh configuration";return false;}
  MeshConfiguration next;if(!parse_configuration(bytes,next,error))return false;
  const auto root=path.parent_path();
  const auto resolve=[&](const std::string& raw,fs::path& resolved){
    std::error_code ec;resolved=fs::canonical(root/fs::path(raw),ec);
    return !ec&&fs::is_regular_file(resolved,ec)&&!ec&&config_detail::local_path(resolved.generic_string());
  };
  if(!resolve(next.certificate_file,next.certificate_path)||!resolve(next.private_key_file,next.private_key_path)||
    !resolve(next.ca_file,next.ca_path)){error="mesh TLS paths must resolve to local regular files";return false;}
  output=std::move(next);return true;
}

inline std::string serialize_policy(const EnrollmentPolicy& policy){
  if(!valid_enrollment_policy(policy)||!policy.revision)return {};
  std::ostringstream out;out<<"CEVMESHPEERS/1\nrevision "<<policy.revision<<"\npeers "<<policy.count<<'\n';
  for(std::size_t i=0;i<policy.count;++i){const auto& p=policy.peers[i];
    out<<"peer "<<p.principal<<' '<<p.certificate_sha256<<' '<<p.generation<<' '<<p.expires_unix_s<<' '<<p.revoked<<' '
      <<static_cast<unsigned>(p.capabilities)<<' '<<p.publication_count<<' '<<p.subscription_count<<' '<<p.artifact_count<<'\n';
    for(std::size_t j=0;j<p.publication_count;++j){const auto& s=p.publications[j];
      out<<"publish "<<s.source<<' '<<s.epoch<<' '<<s.clock<<' '<<s.colony<<'\n';}
    for(std::size_t j=0;j<p.subscription_count;++j){const auto& s=p.subscriptions[j];
      out<<"subscribe "<<s.source<<' '<<s.epoch<<' '<<s.clock<<' '<<s.colony<<'\n';}
    for(std::size_t j=0;j<p.artifact_count;++j)out<<"artifact "<<p.artifact_sha256[j]<<'\n';
    out<<"endpeer\n";
  }
  const auto body=out.str();out<<"seal "<<durable::pl::sha256_hex(body)<<'\n';return out.str();
}
inline bool parse_policy(std::string_view bytes,EnrollmentPolicy& output,std::string& error){
  error.clear();if(bytes.size()>kMaxPolicyBytes){error="mesh policy exceeds bound";return false;}
  EnrollmentPolicy next;config_detail::Lines in(bytes);std::string seal;
  if(!in.row("CEVMESHPEERS/1")||!in.row("revision",next.revision)||!next.revision||
    !in.row("peers",next.count)||next.count>kMaxMeshPeers){error="invalid mesh policy header";return false;}
  for(std::size_t i=0;i<next.count;++i){auto& p=next.peers[i];unsigned revoked=0,capabilities=0;
    if(!in.row("peer",p.principal,p.certificate_sha256,p.generation,p.expires_unix_s,revoked,capabilities,
        p.publication_count,p.subscription_count,p.artifact_count)||revoked>1||capabilities>kMeshCapabilityMask||
      p.publication_count>kMaxMeshScopes||p.subscription_count>kMaxMeshScopes||p.artifact_count>kMaxMeshArtifacts){
      error="invalid mesh peer record";return false;}
    p.revoked=revoked!=0;p.capabilities=static_cast<std::uint8_t>(capabilities);
    for(std::size_t j=0;j<p.publication_count;++j){auto& s=p.publications[j];
      if(!in.row("publish",s.source,s.epoch,s.clock,s.colony)){error="invalid publish scope";return false;}}
    for(std::size_t j=0;j<p.subscription_count;++j){auto& s=p.subscriptions[j];
      if(!in.row("subscribe",s.source,s.epoch,s.clock,s.colony)){error="invalid subscribe scope";return false;}}
    for(std::size_t j=0;j<p.artifact_count;++j)if(!in.row("artifact",p.artifact_sha256[j])){error="invalid artifact grant";return false;}
    if(!in.row("endpeer")){error="unterminated mesh peer";return false;}
  }
  if(!in.row("seal",seal)||!in.end()||!mesh_digest(seal)||!valid_enrollment_policy(next)||serialize_policy(next)!=bytes){
    error="invalid mesh policy seal or canonical encoding";return false;}
  output=std::move(next);return true;
}
inline bool load_policy(const fs::path& path,EnrollmentPolicy& output,std::string& error){
  std::string bytes;if(!durable::read_bounded(path,kMaxPolicyBytes,bytes)){error="cannot read local mesh policy";return false;}
  return parse_policy(bytes,output,error);
}
} // namespace anaiero::mesh::files
