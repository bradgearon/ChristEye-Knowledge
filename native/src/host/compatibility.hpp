#pragma once
#include "anaiero/sdk/evidence.hpp"
namespace anaiero::deployment {
using namespace sdk;
struct Witness {
  Digest build_id,compatibility_id;
  Bytes build_manifest,compatibility_manifest;
  Digest build_sha256,compatibility_sha256;
  bool empty() const noexcept{return build_manifest.empty()&&compatibility_manifest.empty();}
};
struct Binding {
  Digest target_build,compatibility_id;
  std::vector<Witness> origins;
  Witness target;
};
Witness compiled_witness();
Result validate_witness(Witness&);
Bytes encode_binding(const Binding&);
Result decode_binding(std::string_view,Binding&);
// Reads every latest lifecycle source (including retired sources) and every
// schedule, with finite admission bounds. Pending cycles reject migration.
Result session_modules(evidence::ICommitStore&,std::vector<Digest>&,bool require_quiescent);
Result load_binding(evidence::ICommitStore&,Binding&);
Result authorize_binding(evidence::ICommitStore&,const Witness& executing,std::vector<Digest>&);
Result prepare_binding(evidence::ICommitStore&,const Witness& target,const Witness& current,Binding&);
Result stage_binding(evidence::ICommitStore&,const Binding&,evidence::CommitBatch&);
}
