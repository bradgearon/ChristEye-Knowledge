#pragma once
// CEVCELLATLAS/1: strict, sealed, immutable synthetic parameter data.
// Sources such as NWB/NeuroML/cellxgene may be REFERENCED by hash, but are
// not interpreted as validated behavior parameters in this release.
#include "anaiero/contracts/cell_behavior.hpp"
#include "christeye/living/plan_hash.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>
namespace anaiero::cells::files {
namespace fs=std::filesystem;
inline bool number(std::string_view s,std::uint16_t& v) noexcept {
  std::uint32_t n=0;
  const auto [end,error]=std::from_chars(s.data(),s.data()+s.size(),n);
  if(error!=std::errc{}||end!=s.data()+s.size()||n>32)return false;
  v=static_cast<std::uint16_t>(n);return true;
}
inline bool digest(std::string_view s) noexcept {
  if(s.size()!=64)return false;
  for(char c:s)if(!((c>='a'&&c<='f')||(c>='0'&&c<='9')))return false;
  return true;
}
inline bool ident(std::string_view s) noexcept {
  if(s.empty()||s.size()>64)return false;
  for(char c:s)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
                     (c>='0'&&c<='9')||c=='-'||c=='_'))return false;
  return true;
}
inline std::string_view kind_name(Kind k) noexcept {
  switch(k){
    case Kind::crest_progenitor:return "crest_progenitor";
    case Kind::blood_carrier:return "blood_carrier";
    case Kind::neural_progenitor:return "neural_progenitor";
    case Kind::peripheral_neuron:return "peripheral_neuron";
    case Kind::central_neuron:return "central_neuron";
    case Kind::astrocyte:return "astrocyte";
    case Kind::schwann:return "schwann";
    case Kind::microglia:return "microglia";
    case Kind::endothelial:return "endothelial";
  }
  return "invalid";
}
inline bool parse_kind(std::string_view s,Kind& out) noexcept {
  for(unsigned n=1;n<=kKinds;++n){auto k=static_cast<Kind>(n);if(kind_name(k)==s){out=k;return true;}}
  return false;
}
inline bool read_bounded(const fs::path& p,std::size_t limit,std::string& result){
  std::error_code ec;
  if(fs::is_symlink(p,ec)||ec)return false;
  auto size=fs::file_size(p,ec);
  if(ec||size>limit)return false;
  std::ifstream file(p,std::ios::binary);
  if(!file)return false;
  result.assign(std::istreambuf_iterator<char>(file),{});
  return !file.bad()&&result.size()==size;
}
inline Status parse_atlas(std::string_view text,Atlas& output) {
  if(text.size()>32768||!text.ends_with('\n'))return Status::bad_profile;
  const auto pos=text.rfind("seal ");
  if(pos==std::string_view::npos||pos==0||text[pos-1]!='\n'||
     text.size()-pos!=70||text.back()!='\n')return Status::bad_profile;
  const auto hash=text.substr(pos+5,64);
  if(!digest(hash)||christeye::living::plans::sha256_hex(text.substr(0,pos))!=hash)
    return Status::bad_profile;
  std::istringstream in{std::string(text.substr(0,pos))};
  std::string line;
  auto next=[&]() -> bool {return bool(std::getline(in,line)) && !line.empty() && line.back()!='\r';};
  Atlas profile{};
  if(!next()||line!="CEVCELLATLAS/1")return Status::bad_profile;
  if(!next()||!line.starts_with("name ")||!ident(line.substr(5)))return Status::bad_profile;
  profile.name=line.substr(5);
  if(!next()||line!="domain SYNTHETIC_ONLY")return Status::bad_profile;
  if(!next()||line!="model 1")return Status::bad_profile;
  if(!next()||!line.starts_with("reference_sha256 ")||!digest(line.substr(17)))return Status::bad_profile;
  profile.reference_digest=line.substr(17);
  if(!next()||!line.starts_with("reference_file "))return Status::bad_profile;
  profile.reference_file=line.substr(15);
  if(profile.reference_file.empty()||profile.reference_file.size()>100||
     profile.reference_file.find("..")!=std::string::npos)return Status::bad_profile;
  for(char c:profile.reference_file)
    if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||
         (c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))return Status::bad_profile;
  if(!next()||line!="reference_format SYNTHETIC")return Status::bad_profile;
  profile.reference_format="SYNTHETIC";
  std::array<bool,kKinds> known{};
  std::size_t nrows=0;
  while(next()){
    if(line=="end")break;
    std::istringstream row(line);std::vector<std::string> fields;std::string field;
    while(row>>field){if(fields.size()>=12)return Status::bad_profile;fields.push_back(field);}
    if(fields.size()!=11||fields[0]!="cell")return Status::bad_profile;
    Kind kind{};if(!parse_kind(fields[1],kind))return Status::bad_profile;
    const auto index=static_cast<std::size_t>(kind)-1;
    if(known[index])return Status::duplicate;
    known[index]=true;
    auto& p=profile.by_kind[index];p.kind=kind;
    std::array<std::uint16_t*,9> destination{{&p.maintenance,&p.support_response,
      &p.maturation,&p.process_extension,&p.activity_response,&p.plasticity,
      &p.mobility,&p.contact_radius,&p.stress_response}};
    for(std::size_t i=0;i<destination.size();++i)
      if(!number(fields[i+2],*destination[i]))return Status::bad_profile;
    ++nrows;
  }
  if(line!="end"||nrows!=kKinds||next())return Status::bad_profile;
  profile.model_digest=christeye::living::plans::sha256_hex(text);
  output=std::move(profile);return Status::ok;
}
inline Status load_atlas(const fs::path& path,Atlas& output) {
  std::string text;
  if(!read_bounded(path,32768,text))return Status::io_failure;
  return parse_atlas(text,output);
}
} // namespace anaiero::cells::files
