#pragma once
// Scientific source-provenance descriptor; no external dataset is executed or
// treated as a validated biological behavior without independent calibration.
#include "anaiero/infrastructure/cell_atlas_file.hpp"
#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
namespace anaiero::cells::files {
struct SourceRecord {
  std::string name, standard, raw_digest, modality;
  std::string unit, clock, uncertainty, classification, record_digest;
};
inline bool known_standard(std::string_view v) noexcept {
  return v=="SYNTHETIC"||v=="NWB"||v=="NEUROML"||v=="CELLXGENE"||
         v=="OME_ZARR"||v=="SONATA";
}
inline Status parse_source(std::string_view text,SourceRecord& result) {
  if(text.size()>8192||!text.ends_with('\n'))return Status::bad_profile;
  const auto seal=text.rfind("seal ");
  if(seal==std::string_view::npos||seal==0||text[seal-1]!='\n'||
     text.size()-seal!=70||!digest(text.substr(seal+5,64))||
     christeye::living::plans::sha256_hex(text.substr(0,seal))!=text.substr(seal+5,64))
    return Status::bad_profile;
  SourceRecord candidate;
  std::istringstream input{std::string(text.substr(0,seal))};std::string line;
  auto field=[&](std::string_view key,std::string& value)->bool {
    if(!std::getline(input,line)||line.size()<=key.size()+1||
       !line.starts_with(std::string(key)+" "))return false;
    value=line.substr(key.size()+1);
    return ident(value);
  };
  if(!std::getline(input,line)||line!="CEVCELLSOURCE/1"||
     !field("name",candidate.name)||!field("standard",candidate.standard)||
     !known_standard(candidate.standard))return Status::bad_profile;
  if(!std::getline(input,line)||!line.starts_with("raw_sha256 ")||
     !digest(line.substr(11)))return Status::bad_profile;
  candidate.raw_digest=line.substr(11);
  if(!field("modality",candidate.modality)||!field("units",candidate.unit)||
     !field("clock",candidate.clock)||!field("uncertainty",candidate.uncertainty)||
     !field("classification",candidate.classification))return Status::bad_profile;
  if(candidate.standard=="SYNTHETIC"){
    if(candidate.classification!="SYNTHETIC_ONLY")return Status::bad_profile;
  }else if(candidate.classification!="RESEARCH_UNVERIFIED")return Status::bad_profile;
  if(!std::getline(input,line)||line!="end"||std::getline(input,line))
    return Status::bad_profile;
  candidate.record_digest=christeye::living::plans::sha256_hex(text);
  result=std::move(candidate);return Status::ok;
}
inline Status load_source(const std::filesystem::path& path,SourceRecord& output){
  std::string bytes;
  if(!read_bounded(path,8192,bytes))return Status::io_failure;
  return parse_source(bytes,output);
}
inline Status validate_evidence(const Atlas& atlas,const SourceRecord& source) noexcept {
  if(atlas.reference_digest!=source.record_digest)return Status::bad_profile;
  if(atlas.synthetic_only&&source.classification!="SYNTHETIC_ONLY")return Status::bad_profile;
  return Status::ok;
}
inline Status load_checked_atlas(const std::filesystem::path& atlas_file,Atlas& output) {
  Atlas draft;
  const auto parsed=load_atlas(atlas_file,draft);
  if(parsed!=Status::ok)return parsed;
  SourceRecord source;
  const auto status=load_source(atlas_file.parent_path()/draft.reference_file,source);
  if(status!=Status::ok)return status;
  if(validate_evidence(draft,source)!=Status::ok)return Status::bad_profile;
  output=std::move(draft);
  return Status::ok;
}
} // namespace anaiero::cells::files
