#pragma once
#include "anaiero/models/passive_membrane.hpp"
#include "christeye/living/durable_records.hpp"
#include <filesystem>
#include <sstream>
namespace anaiero::science::files {
inline bool parse_passive_model(std::string_view bytes,PassiveMembraneModel& model,std::string& sha){
  if(bytes.size()>4096)return false;
  std::istringstream input{std::string(bytes)};std::string header,rest,tau,resistance,current,end,extra;
  PassiveMembraneModel next{};
  if(!(input>>header>>rest>>next.rest_microvolt>>tau>>next.tau_ns>>resistance>>next.resistance_ohm>>current>>next.max_current_picoamp>>end)||
     input>>extra||header!="CEPASSIVE/1"||rest!="rest_microvolt"||tau!="tau_ns"||
     resistance!="resistance_ohm"||current!="max_current_picoamp"||end!="end")return false;
  model=next;sha=christeye::living::plans::sha256_hex(bytes);return true;
}
inline bool read_passive_model(const std::filesystem::path& path,PassiveMembraneModel& model,std::string& sha){
  std::string bytes;
  return christeye::living::durable::read_bounded(path,4096,bytes)&&
    parse_passive_model(bytes,model,sha);
}
} // namespace anaiero::science::files
