#pragma once
#include "anaiero/models/passive_membrane.hpp"
#include "christeye/living/durable_records.hpp"
#include <filesystem>
#include <sstream>
namespace anaiero::science::files {
inline bool read_passive_model(const std::filesystem::path& path,PassiveMembraneModel& model,std::string& sha){
  std::string bytes;
  if(!christeye::living::durable::read_bounded(path,4096,bytes))return false;
  std::istringstream input(bytes);std::string header,rest,tau,resistance,current,end,extra;
  PassiveMembraneModel next{};
  if(!(input>>header>>rest>>next.rest_microvolt>>tau>>next.tau_ns>>resistance>>next.resistance_ohm>>current>>next.max_current_picoamp>>end)||
     input>>extra||header!="CEPASSIVE/1"||rest!="rest_microvolt"||tau!="tau_ns"||
     resistance!="resistance_ohm"||current!="max_current_picoamp"||end!="end")return false;
  model=next;sha=christeye::living::plans::sha256_hex(bytes);return true;
}
} // namespace anaiero::science::files
