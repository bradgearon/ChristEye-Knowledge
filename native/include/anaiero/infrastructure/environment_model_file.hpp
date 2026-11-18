#pragma once
#include "anaiero/models/environment_compartment.hpp"
#include "christeye/living/durable_records.hpp"
#include <filesystem>
#include <locale>
#include <sstream>
namespace anaiero::science::files {
inline std::string serialize_environment_model(const EnvironmentCompartmentModel& model){
  std::ostringstream output;output.imbue(std::locale::classic());
  output<<"CEENVIRONMENT/1\ninitial_micromolar "<<model.initial_micromolar
    <<"\ninlet_max_micromolar "<<model.inlet_max_micromolar
    <<"\nexchange_tau_ns "<<model.exchange_tau_ns
    <<"\nconsumption_ppm_per_second "<<model.consumption_ppm_per_second<<"\nend\n";
  return output.str();
}
inline bool read_environment_model(const std::filesystem::path& path,EnvironmentCompartmentModel& model,std::string& sha){
  std::string bytes;
  if(!christeye::living::durable::read_bounded(path,4096,bytes))return false;
  std::istringstream input(bytes);input.imbue(std::locale::classic());
  std::string header,initial,inlet,tau,consumption,end,extra;
  EnvironmentCompartmentModel next{};
  if(!(input>>header>>initial>>next.initial_micromolar>>inlet>>next.inlet_max_micromolar>>
       tau>>next.exchange_tau_ns>>consumption>>next.consumption_ppm_per_second>>end)||input>>extra||
     header!="CEENVIRONMENT/1"||initial!="initial_micromolar"||inlet!="inlet_max_micromolar"||
     tau!="exchange_tau_ns"||consumption!="consumption_ppm_per_second"||end!="end")return false;
  // Only syntax and byte provenance belong to this file adapter. Numerical
  // bounds are enforced by EnvironmentCompartmentService before any use.
  model=next;sha=christeye::living::plans::sha256_hex(bytes);return true;
}
} // namespace anaiero::science::files
