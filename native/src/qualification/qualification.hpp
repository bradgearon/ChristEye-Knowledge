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
struct Profile {
  std::string name,storage_label;
  std::uint64_t observations=0,epochs=0,batch_size=0,payload_bytes=0;
  std::uint64_t query_samples=0,maximum_store_bytes=0,maximum_files=0;
  std::uint64_t maximum_resident_bytes=0,maximum_disk_bytes=0;
  std::uint64_t maximum_commit_p99_us=0,maximum_lookup_p99_us=0;
  std::uint64_t maximum_reopen_us=0,maximum_lookup_nodes=0;
  std::vector<std::uint64_t> checkpoints;
};
struct Histogram {
  std::array<std::uint64_t,65> buckets{};
  std::uint64_t count=0,minimum_ns=0,maximum_ns=0;
  void add(std::uint64_t ns) noexcept;
  std::uint64_t percentile_upper_ns(unsigned percent)const noexcept;
};
struct Resources {
  std::uint64_t resident_bytes=0,peak_resident_bytes=0,cpu_ns=0;
  std::uint64_t voluntary_context_switches=0,involuntary_context_switches=0;
  bool context_switches_available=false;
};
struct DiskUsage {
  std::uint64_t files=0,logical_bytes=0,allocated_bytes=0;
  bool allocated_bytes_available=false;
};
struct Environment {
  std::string operating_system,architecture,filesystem,compiler,build_identity;
  std::uint64_t logical_processors=0;
};
struct Checkpoint {
  std::uint64_t observations=0,store_generation=0,accounted_bytes=0;
  std::uint64_t lookup_maximum_nodes=0,reopen_ns=0,reopen_nodes=0;
  Histogram lookup;
  Resources resources;
  DiskUsage disk;
};
struct Report {
  std::string profile_sha256,started_utc,finished_utc,store_id;
  Profile profile;
  Environment environment;
  Histogram commit;
  Resources before,after;
  std::vector<Checkpoint> checkpoints;
  std::vector<std::string> violations;
  DiskUsage final_disk;
  std::uint64_t observations=0,epochs=0,maximum_active_sources=0;
  std::uint64_t wall_ns=0,ingestion_ns=0,audit_ns=0,audit_generations=0,audit_records=0;
  std::uint64_t final_generation=0,final_accounted_bytes=0;
  std::uint64_t write_index_reads=0,write_index_nodes=0,staged_index_nodes=0;
  std::uint64_t transient_nodes_elided=0,index_spills=0,index_peak_nodes=0,index_peak_bytes=0;
  std::uint64_t audit_canonical_records=0,audit_index_nodes=0;
  bool audit_passed=false,restart_identity_verified=false,passed=false;
};
sdk::Result parse_profile(std::string_view,Profile&);
sdk::Result read_resources(Resources&);
sdk::Result measure_disk(std::string_view directory,std::uint64_t maximum_files,DiskUsage&);
sdk::Result read_environment(std::string_view directory,Environment&);
sdk::Result run_profile(const Profile&,std::string_view directory,Report&,
                       std::function<void(std::uint64_t)> progress={});
std::string report_json(const Report&);
}
