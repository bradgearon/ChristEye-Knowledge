#pragma once
#include "deployment.hpp"
namespace anaiero::deployment {
struct PackagePolicy {
  std::uint64_t maximum_packages=16,maximum_bytes=128U*1024U*1024U;
  std::vector<Digest> packages;
};
struct PackageUsage {PackagePolicy policy;std::uint64_t retained_bytes=0;};
struct PackageRetentionPlan {
  std::string operation_id,store_id;
  std::uint64_t generation=0,bytes=0;
  Digest root_sha256,plan_sha256;
  std::vector<Digest> packages,objects;
};
Result package_usage(evidence::ICommitStore&,PackageUsage&);
Result package_policy(evidence::ICommitStore&,std::uint64_t count,std::uint64_t bytes,std::string_view operation,evidence::CommitReceipt&);
Result package_pin(evidence::ICommitStore&,std::string_view manifest,bool pinned,std::string_view operation,evidence::CommitReceipt&);
Result stage_package_admission(evidence::ICommitStore&,const VerifiedPackage&,evidence::CommitBatch&);
Bytes encode_package_retention(const PackageRetentionPlan&);
Result decode_package_retention(std::string_view,PackageRetentionPlan&);
Result read_package_retention(std::string_view path,PackageRetentionPlan&);
Result package_retention_review(evidence::ICommitStore&,std::vector<Digest>,std::string_view operation,PackageRetentionPlan&);
Result package_retention_activate(evidence::ICommitStore&,const PackageRetentionPlan&,evidence::CommitReceipt&);
Result package_retention_reclaim(evidence::ICommitStore&,std::string_view operation,std::uint64_t& remaining);
}
