#include "compatibility.hpp"
#include "anaiero_build_witness.hpp"
namespace anaiero::deployment {
Witness compiled_witness() {
  Bytes build,compatible;
  for(const auto part:compiled::build_manifest)build+=part;
  for(const auto part:compiled::compatibility_manifest)compatible+=part;
  return {std::string(compiled::build_id),std::string(compiled::compatibility_id),
    std::move(build),std::move(compatible),{}, {}};
}
}
