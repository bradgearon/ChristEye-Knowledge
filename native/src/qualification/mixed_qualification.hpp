#pragma once
#include "qualification.hpp"
#include "anaiero/sdk/host.hpp"

namespace anaiero::qualification {
// The finite built-in model horizons remain part of admission. Long-duration
// profiles change wall pacing, never the numerical or source clock semantics.
struct MixedProfile {
  std::string name,storage_label;
  std::uint64_t cycles=0,period_ms=0,restart_after_cycle=0,checkpoint_cycles=0;
  std::uint64_t blocked_lane_ms=0,minimum_run_ms=0,maximum_run_ms=0;
  std::uint64_t maximum_store_bytes=0,maximum_disk_bytes=0,maximum_files=0;
  std::uint64_t maximum_resident_bytes=0,maximum_status_p99_us=0;
  std::uint64_t maximum_control_p99_us=0,maximum_probe_p99_us=0;
  std::uint64_t maximum_reopen_us=0,maximum_lookup_nodes=0;
};
struct MixedEngine {
  sdk::EngineIdentity identity;
  std::string configuration_sha256,input_sha256,state_sha256;
  std::uint64_t cadence=0,tick=0,revision=0;
  sdk::Phase phase=sdk::Phase::ready;
};
struct MixedReport {
  MixedProfile profile;
  std::string profile_sha256,session_text_sha256,session_definition_sha256;
  std::string started_utc,finished_utc,store_id;
  Environment environment;
  Resources before,after;
  DiskUsage disk;
  Histogram status,control,probe;
  std::vector<MixedEngine> engines;
  std::vector<std::string> violations;
  std::uint64_t cycles=0,wall_ns=0,workload_ns=0,reopen_ns=0,audit_ns=0;
  std::uint64_t final_generation=0,final_accounted_bytes=0,lookup_maximum_nodes=0;
  std::uint64_t sampled_peak_disk_bytes=0,sampled_peak_files=0;
  std::uint64_t owner_wakeups=0,completed_tasks=0,probes=0;
  std::uint64_t maximum_queued_commands=0,maximum_queued_tasks=0;
  std::uint64_t task_rejections=0,command_rejections=0;
  bool pressure_verified=false,control_retry_verified=false,quota_rejection_verified=false;
  bool restart_identity_verified=false,finite_plan_completed=false,audit_passed=false,passed=false;
};
sdk::Result parse_mixed_profile(std::string_view,MixedProfile&);
sdk::Result validate_mixed_definition(const sdk::SessionDefinition&,const MixedProfile&);
sdk::Result run_mixed_profile(const MixedProfile&,const sdk::SessionDefinition&,
    std::string_view directory,MixedReport&,std::function<void(std::uint64_t)> progress={});
std::string mixed_report_json(const MixedReport&);
}
