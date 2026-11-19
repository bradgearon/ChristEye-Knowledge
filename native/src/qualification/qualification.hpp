#pragma once
// Private qualification records and ports. The production SDK does not depend
// on the benchmark harness or platform resource measurement mechanisms.
#include "anaiero/sdk/evidence.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace anaiero::qualification {
inline constexpr std::string_view histogram_percentile_method="log2-ceiling-capped-by-observed-maximum/1";
inline constexpr std::string_view histogram_percentile_description=
    "nearest-rank upper bounds in nanoseconds: min(log2 bucket ceiling, exact observed maximum)";
inline constexpr std::string_view threshold_method="exact-nearest-rank-threshold/1";
struct Profile {
  std::string name,storage_label;
  std::string latency_gate{histogram_percentile_method};
  std::uint64_t observations=0,epochs=0,batch_size=0,payload_bytes=0;
  std::uint64_t query_samples=0,maximum_store_bytes=0,maximum_files=0;
  std::uint64_t maximum_resident_bytes=0,maximum_disk_bytes=0;
  std::uint64_t maximum_commit_p99_us=0,maximum_lookup_p99_us=0;
  std::uint64_t maximum_reopen_us=0,maximum_lookup_nodes=0;
  std::vector<std::uint64_t> checkpoints;
};
struct Histogram {
  std::array<std::uint64_t,65> buckets{};
  std::uint64_t count=0,minimum_ns=0,maximum_ns=0,sum_ns=0;
  bool sum_saturated=false;
  void add(std::uint64_t ns) noexcept;
  std::uint64_t percentile_upper_ns(unsigned percent)const noexcept;
};
// Exact inclusive threshold decision with fixed storage. No sample history is
// needed: p99 <= limit iff no more than floor(n/100) samples exceed the limit.
struct ThresholdCounter {
  std::uint64_t limit_ns=0,count=0,exceeded=0;
  void add(std::uint64_t ns) noexcept;
  std::uint64_t allowed_exceedances(unsigned percent)const noexcept;
  bool passes(unsigned percent)const noexcept;
};
struct DurabilityMeasurements {
  Histogram lock_wait,validation,staging,blobs,transaction,root,index,cleanup,total;
};
// CPU and scheduler counters for the calling thread. Sample outside the
// stopwatch so the qualification observer does not enter the measured commit.
struct ThreadActivity {
  std::uint64_t cpu_ns=0,voluntary_switches=0,involuntary_switches=0;
  bool cpu_available=false,switches_available=false;
};
struct CommitSample {
  std::uint64_t batch=0,first_observation=0,last_observation=0,expected_generation=0;
  std::uint64_t elapsed_ns=0;
  sdk::evidence::CommitTimings stages;
  sdk::evidence::IndexStatistics index_work;
  ThreadActivity thread;
};
struct SlowestCommits {
  static constexpr std::size_t maximum=32;
  std::array<CommitSample,maximum> samples{};
  std::size_t size=0;
  void add(const CommitSample&) noexcept;
};
struct Resources {
  std::uint64_t resident_bytes=0,peak_resident_bytes=0,cpu_ns=0;
  std::uint64_t voluntary_context_switches=0,involuntary_context_switches=0;
  bool context_switches_available=false;
};
struct DiskUsage {
  std::uint64_t files=0,logical_bytes=0,allocated_bytes=0;
  bool allocated_bytes_available=false;
  std::uint64_t directories=0,directory_allocated_bytes=0,allocation_unit_bytes=0;
  std::uint64_t filesystem_total_bytes=0,filesystem_available_bytes=0;
  std::uint64_t filesystem_file_nodes_available=0;
  bool directory_allocation_available=false,file_nodes_available=false;
};
struct DeviceDeclaration {
  std::string name,device_class,machine,processor,storage_device,storage_firmware;
  std::string storage_controller,filesystem,mount_options,durability,dependencies,concurrent_work;
  std::uint64_t memory_bytes=0;
};
struct Environment {
  std::string operating_system,architecture,filesystem,compiler,build_identity;
  std::uint64_t logical_processors=0;
};
struct Checkpoint {
  std::uint64_t observations=0,store_generation=0,accounted_bytes=0;
  std::uint64_t lookup_maximum_nodes=0,reopen_ns=0,reopen_nodes=0;
  Histogram lookup;
  ThresholdCounter lookup_threshold;
  Resources resources;
  DiskUsage disk;
};
struct Report {
  std::string profile_sha256,started_utc,finished_utc,store_id;
  Profile profile;
  Environment environment;
  Histogram commit;
  ThresholdCounter commit_threshold;
  DurabilityMeasurements durability;
  SlowestCommits slowest_commits;
  sdk::evidence::IndexLayout requested_layout=sdk::evidence::IndexLayout::automatic;
  std::string index_backend;
  DeviceDeclaration device;
  std::string device_sha256;
  Resources before,after;
  std::vector<Checkpoint> checkpoints;
  std::vector<std::string> violations;
  DiskUsage final_disk;
  DiskUsage admission_disk;
  std::uint64_t minimum_file_nodes=0;
  std::uint64_t observations=0,epochs=0,maximum_active_sources=0;
  std::uint64_t wall_ns=0,ingestion_ns=0,audit_ns=0,audit_generations=0,audit_records=0;
  std::uint64_t final_generation=0,final_accounted_bytes=0;
  std::uint64_t write_index_reads=0,write_index_nodes=0,staged_index_nodes=0;
  std::uint64_t transient_nodes_elided=0,index_spills=0,index_peak_nodes=0,index_peak_bytes=0;
  std::uint64_t admission_cache_nodes_reused=0;
  std::uint64_t audit_canonical_records=0,audit_index_nodes=0;
  bool audit_passed=false,restart_identity_verified=false,passed=false;
};
sdk::Result parse_profile(std::string_view,Profile&);
sdk::Result parse_device_declaration(std::string_view,DeviceDeclaration&);
std::string device_declaration_json(const DeviceDeclaration&);
std::string disk_usage_json(const DiskUsage&);
std::string threshold_json(const ThresholdCounter&,unsigned percent);
std::string_view layout_label(sdk::evidence::IndexLayout) noexcept;
ThreadActivity read_thread_activity() noexcept;
ThreadActivity activity_delta(const ThreadActivity& before,const ThreadActivity& after) noexcept;
sdk::Result read_resources(Resources&);
sdk::Result measure_disk(std::string_view directory,std::uint64_t maximum_files,DiskUsage&);
sdk::Result require_file_nodes(const DiskUsage&,std::uint64_t minimum);
sdk::Result read_environment(std::string_view directory,Environment&);
sdk::Result run_profile(const Profile&,std::string_view directory,Report&,
                       std::function<void(std::uint64_t)> progress={},
                       sdk::evidence::IndexLayout layout=sdk::evidence::IndexLayout::automatic);
std::string report_json(const Report&);
}
