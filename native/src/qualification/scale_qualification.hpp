#pragma once
#include "qualification.hpp"
#include "anaiero/sdk/lifecycle.hpp"
#include "anaiero/sdk/storage_tools.hpp"

namespace anaiero::qualification {
struct ScaleProfile {
  Profile evidence;
  std::uint64_t receiver_pages_per_checkpoint=0,receiver_page_observations=0;
  std::uint64_t retained_receiver_pages=0,compaction_working_bytes=0;
};
struct ScaleLookup {
  std::uint64_t sequence=0,maximum_nodes=0;
  Histogram latency;
  ThresholdCounter threshold;
};
struct ScaleCheckpoint {
  std::uint64_t observations=0,epochs=0,receiver_observations=0,receiver_expired=0;
  std::uint64_t generation=0,reopen_ns=0,reopen_nodes=0,audit_ns=0;
  std::uint64_t compaction_ns=0,required_working_bytes=0;
  std::uint64_t retained_records=0,retained_blobs=0,retained_operations=0;
  sdk::Digest root;
  DiskUsage before_disk,after_disk;
  Resources resources;
  sdk::lifecycle::CapacityStatus capacity;
  sdk::evidence::CompactionReport compaction;
  sdk::evidence::AuditReport audit;
  ScaleLookup oldest,middle,recent;
  bool restart_verified=false,identity_fences_verified=false,exact_retry_verified=false;
};
struct ScaleReport {
  ScaleProfile profile;
  std::string profile_sha256,started_utc,finished_utc,store_id;
  Environment environment;
  DeviceDeclaration device;
  std::string device_sha256;
  Resources before,after;
  DiskUsage admission_disk,final_disk;
  std::uint64_t minimum_file_nodes=0;
  Histogram commit;
  ThresholdCounter commit_threshold,lookup_threshold;
  std::uint64_t observations=0,epochs=0,receiver_observations=0,receiver_expired=0;
  std::uint64_t wall_ns=0,maximum_active_sources=0,reclaimed_payload_bytes=0;
  std::uint64_t maximum_retained_receiver_observations=0;
  std::vector<ScaleCheckpoint> checkpoints;
  std::vector<std::string> violations;
  bool protected_root_verified=false,quota_rejection_verified=false;
  bool maintenance_reserve_verified=false,passed=false;
};
sdk::Result parse_scale_profile(std::string_view,ScaleProfile&);
sdk::Result run_scale_profile(const ScaleProfile&,std::string_view directory,ScaleReport&,
                             std::function<void(std::uint64_t)> progress={});
std::string scale_report_json(const ScaleReport&);
}
