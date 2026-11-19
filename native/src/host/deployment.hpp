#pragma once
// Private deployment contracts. Package bytes are verified by the native
// adapter; selection and retry rules belong to the canonical service.
#include "anaiero/sdk/evidence.hpp"
#include "compatibility.hpp"

namespace anaiero::deployment {
using namespace sdk;
struct Package {
  std::string directory;
  Digest manifest_sha256,host_sha256,module_sha256;
  bool operator==(const Package&) const=default;
};
struct VerifiedPackage {
  Package identity;
  Bytes manifest,host;
  std::uint64_t files=0,bytes=0;
  Witness witness;
};
struct Selection {
  Package current,previous;
  std::string operation_id;
  Digest plan_sha256;
  std::uint64_t revision=0,generation=0;
  Digest record_sha256;
};
struct Plan {
  std::string operation_id,store_id;
  std::uint64_t generation=0,selection_revision=0;
  Digest root_sha256,selection_sha256,plan_sha256;
  Package candidate,previous;
  Bytes compatibility_binding;
};
Bytes encode_plan(const Plan&);
Result decode_plan(std::string_view,Plan&);
class Service {
  evidence::ICommitStore& store_;
  Result review_impl(const Package&,std::string_view,Plan&,bool compatible);
public:
  explicit Service(evidence::ICommitStore& store):store_(store){}
  Result selection(Selection&);
  Result retained(const Package&);
  Result witness(const Package&,Witness&);
  Result review(const Package&,std::string_view operation,Plan&);
  Result review(const VerifiedPackage&,std::string_view operation,Plan&);
  // A duplicate receipt is returned even if a later selection superseded it.
  // Repeating an activation never reselects an old package.
  Result reconcile(const Plan&,evidence::CommitReceipt&);
  Result activate(const Plan&,const VerifiedPackage&,evidence::CommitReceipt&);
};

// Native immutable package admission. Bounds: 4 MiB manifest, 8192 entries,
// 512 MiB per installed file, 8 GiB total, 8 MiB retained host executable.
Result verify_native_package(std::string_view directory,std::string_view module,VerifiedPackage&,bool probe=true);
Result read_native_plan(std::string_view path,Plan&);
Result open_deployment_store(std::string_view directory,std::unique_ptr<evidence::ICommitStore>&);
Result verify_selected_host(evidence::ICommitStore&,std::string_view executable,std::string_view module);
std::string selected_executable(const Package&);
std::string json_string(std::string_view);
}
