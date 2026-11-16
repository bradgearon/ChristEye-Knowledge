#pragma once
// ChristEye Anaiero SIM3.2: read-only, synthetic-only prediction evaluation.
// No neural stimulation, biology control, diagnostic inference, or genetics model.
// CELAW/1 is a DATA PROFILE, not executable code or a dynamic library.
#include "christeye/living/colony_runner.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace christeye::living::fidelity {
namespace fs = std::filesystem;
namespace sim = christeye::living::sim3;
namespace pl = christeye::living::plans;

inline constexpr std::size_t kMaxManifestBytes = 65536;
inline constexpr std::size_t kMaxRecords = 512;
inline constexpr std::uint32_t kMaxEvaluationTick = 4096;

enum class Status : std::uint8_t {
  ok, io_error, invalid_format, invalid_seal, untrusted_path, bad_digest,
  config_mismatch, law_mismatch, bad_dataset, missing_checkpoint, bad_snapshot
};
struct Result { Status code = Status::ok; std::size_t line = 0; };

inline bool is_metric(std::string_view name) noexcept {
  return name == "alive" || name == "energy" || name == "learning" ||
         name == "food" || name == "stress" || name == "maturity";
}
inline bool is_parameter(std::string_view name) noexcept {
  return name == "food_per_cell" || name == "fed_gain" ||
         name == "unfed_cost" || name == "upkeep_cost" ||
         name == "stress_cutoff" || name == "stress_cost" ||
         name == "maturity_gain" || name == "blood_step" ||
         name == "learning_gain";
}
inline int law_value(const sim::ModelLaws& laws, std::string_view name) noexcept {
  if (name == "food_per_cell") return laws.food_per_cell;
  if (name == "fed_gain") return laws.fed_gain;
  if (name == "unfed_cost") return laws.unfed_cost;
  if (name == "upkeep_cost") return laws.upkeep_cost;
  if (name == "stress_cutoff") return laws.stress_cutoff;
  if (name == "stress_cost") return laws.stress_cost;
  if (name == "maturity_gain") return laws.maturity_gain;
  if (name == "blood_step") return laws.blood_step;
  if (name == "learning_gain") return laws.learning_gain;
  return -1;
}
inline bool tokenize(const std::string& line, std::vector<std::string>& words) {
  if (line.size() > 380) return false;
  std::istringstream source(line);
  std::string value;
  while (source >> value) {
    if (words.size() == 10 || value.size() > 180) return false;
    words.push_back(std::move(value));
  }
  return !words.empty();
}
inline Result sealed_lines(std::string_view input, std::vector<std::vector<std::string>>& lines) {
  if (input.empty() || input.size() > kMaxManifestBytes || input.back() != '\n')
    return {Status::invalid_format, 0};
  auto end = input.rfind("seal ");
  if (end == std::string_view::npos || end == 0 || input[end - 1] != '\n')
    return {Status::invalid_seal, 0};
  const auto digest = input.substr(end + 5);
  if (digest.size() != 65 || digest.back() != '\n' ||
      !sim::digest(digest.substr(0, 64)) ||
      pl::sha256_hex(input.substr(0, end)) != digest.substr(0, 64))
    return {Status::invalid_seal, 0};
  std::istringstream source{std::string(input.substr(0, end))};
  std::string line;
  std::size_t number = 0;
  while (std::getline(source, line)) {
    ++number;
    if (line.empty() || line.front() == '#' || line.back() == '\r')
      return {Status::invalid_format, number};
    std::vector<std::string> tokens;
    if (!tokenize(line, tokens)) return {Status::invalid_format, number};
    lines.push_back(std::move(tokens));
    if (lines.size() > 520) return {Status::invalid_format, number};
  }
  return {};
}
inline bool pinned_file(const fs::path& trusted_root, const std::string& relative,
                        const std::string& hash, std::string& content) {
  if (!sim::safe_relative(relative) || !sim::digest(hash)) return false;
  fs::path actual;
  if (!sim::bound_path(trusted_root, relative, actual)) return false;
  if (!sim::bounded_read(actual, kMaxManifestBytes, content)) return false;
  return pl::sha256_hex(content) == hash;
}

struct LawProfile {
  std::string name, configuration_hash, sha256;
  std::uint32_t version = 0;
  std::map<std::string, std::uint32_t> parameters;
  static Result parse(std::string_view content, LawProfile& output) {
    std::vector<std::vector<std::string>> lines;
    if (auto status = sealed_lines(content, lines); status.code != Status::ok) return status;
    if (lines.size() != 15 || lines[0] != std::vector<std::string>{"CELAW/1"} ||
        lines[1].size() != 2 || lines[1][0] != "name" || !sim::ident(lines[1][1]) ||
        lines[2] != std::vector<std::string>{"domain", "SYNTHETIC_ONLY"} ||
        lines[3].size() != 2 || lines[3][0] != "version" || lines[3][1] != "1" ||
        lines[4].size() != 2 || lines[4][0] != "config_sha256" || !sim::digest(lines[4][1]) ||
        lines.back() != std::vector<std::string>{"end"}) return {Status::invalid_format, 0};
    LawProfile draft;
    draft.name = lines[1][1]; draft.version = 1;
    draft.configuration_hash = lines[4][1];
    for (std::size_t i = 5; i < lines.size() - 1; ++i) {
      const auto& t = lines[i];
      std::uint32_t v = 0;
      if (t.size() != 3 || t[0] != "parameter" || !is_parameter(t[1]) ||
          !sim::parse_u32(t[2], v) || v > 100 ||
          !draft.parameters.emplace(t[1], v).second)
        return {Status::invalid_format, i + 1};
    }
    if (draft.parameters.size() != 9) return {Status::invalid_format, 0};
    draft.sha256 = pl::sha256_hex(content);
    output = std::move(draft);
    return {};
  }
  bool matches(const sim::Configuration& configuration) const noexcept {
    if (configuration.hash != configuration_hash ||
        configuration.laws.version != version || parameters.size() != 9) return false;
    for (const auto& [parameter_name, value] : parameters)
      if (law_value(configuration.laws, parameter_name) != static_cast<int>(value)) return false;
    return true;
  }
};

struct Observation {
  std::uint32_t tick = 0;
  std::string colony, metric, split;
  std::int64_t measured = 0, radius = 0;
};
struct Dataset {
  std::string name, config_hash, source_hash, sha256;
  std::vector<Observation> rows;
  static Result parse(std::string_view content, Dataset& output) {
    std::vector<std::vector<std::string>> lines;
    if (auto status = sealed_lines(content, lines); status.code != Status::ok) return status;
    if (lines.size() < 10 || lines[0] != std::vector<std::string>{"CEVOBS/1"} ||
        lines[1].size() != 2 || lines[1][0] != "name" || !sim::ident(lines[1][1]) ||
        lines[2] != std::vector<std::string>{"origin", "SYNTHETIC_FIXTURE"} ||
        lines[3].size() != 2 || lines[3][0] != "config_sha256" || !sim::digest(lines[3][1]) ||
        lines[4].size() != 2 || lines[4][0] != "source_sha256" || !sim::digest(lines[4][1]) ||
        lines[5] != std::vector<std::string>{"units", "DIMENSIONLESS_PROXY"} ||
        lines.back() != std::vector<std::string>{"end"}) return {Status::invalid_format, 0};
    Dataset draft; draft.name = lines[1][1]; draft.config_hash = lines[3][1];
    draft.source_hash = lines[4][1];
    std::set<std::string> unique;
    std::set<std::string> training_metrics;
    std::set<std::string> holdout_metrics;
    std::size_t training = 0, held_out = 0;
    for (std::size_t i = 6; i + 1 < lines.size(); ++i) {
      const auto& t = lines[i];
      std::uint32_t tick = 0, value = 0, radius = 0;
      if (t.size() != 7 || t[0] != "row" || !sim::parse_u32(t[1], tick) ||
          tick > kMaxEvaluationTick || !(t[2] == "all" || sim::ident(t[2])) ||
          !is_metric(t[3]) || !sim::parse_u32(t[4], value) ||
          !sim::parse_u32(t[5], radius) || radius == 0 || radius > 100000 ||
          !(t[6] == "train" || t[6] == "holdout") ||
          !unique.insert(std::to_string(tick) + "/" + t[2] + "/" + t[3]).second)
        return {Status::bad_dataset, i + 1};
      if (t[6] == "train") {++training; training_metrics.insert(t[2] + "/" + t[3]);}
      else {++held_out; holdout_metrics.insert(t[2] + "/" + t[3]);}
      draft.rows.push_back({tick,t[2],t[3],t[6],value,radius});
      if (draft.rows.size() > kMaxRecords) return {Status::bad_dataset, i + 1};
    }
    if (training < 2 || held_out < 2 || draft.rows.empty()) return {Status::bad_dataset, 0};
    for (const auto& key : holdout_metrics)
      if (!training_metrics.contains(key)) return {Status::bad_dataset, 0};
    draft.sha256 = pl::sha256_hex(content);
    output = std::move(draft);
    return {};
  }
};

struct Manifest {
  std::string name, config_hash, law_file, law_sha256, obs_file, obs_sha256, sha256;
  static Result parse(std::string_view content, Manifest& output) {
    std::vector<std::vector<std::string>> lines;
    if (auto status = sealed_lines(content, lines); status.code != Status::ok) return status;
    if (lines.size() != 7 || lines[0] != std::vector<std::string>{"CEVEVAL/1"} ||
        lines[1].size() != 2 || lines[1][0] != "name" || !sim::ident(lines[1][1]) ||
        lines[2] != std::vector<std::string>{"domain", "SYNTHETIC_ONLY"} ||
        lines[3].size() != 2 || lines[3][0] != "config_sha256" || !sim::digest(lines[3][1]) ||
        lines[4].size() != 3 || lines[4][0] != "law_file" ||
        !sim::safe_relative(lines[4][1]) || !sim::digest(lines[4][2]) ||
        lines[5].size() != 3 || lines[5][0] != "observations" ||
        !sim::safe_relative(lines[5][1]) || !sim::digest(lines[5][2]) ||
        lines[6] != std::vector<std::string>{"end"}) return {Status::invalid_format, 0};
    Manifest draft;
    draft.name = lines[1][1]; draft.config_hash = lines[3][1];
    draft.law_file = lines[4][1]; draft.law_sha256 = lines[4][2];
    draft.obs_file = lines[5][1]; draft.obs_sha256 = lines[5][2];
    draft.sha256 = pl::sha256_hex(content);
    output = std::move(draft);
    return {};
  }
};

struct Bundle {
  Manifest manifest;
  LawProfile laws;
  Dataset dataset;
  static Status load(const sim::Configuration& configuration,
                     const fs::path& manifest_file, const fs::path& trusted_root,
                     Bundle& output) {
    std::string bytes;
    if (!sim::bounded_read(manifest_file, kMaxManifestBytes, bytes)) return Status::io_error;
    Bundle draft;
    if (Manifest::parse(bytes,draft.manifest).code != Status::ok) return Status::invalid_seal;
    if (draft.manifest.config_hash != configuration.hash) return Status::config_mismatch;
    if (!pinned_file(trusted_root, draft.manifest.law_file, draft.manifest.law_sha256,bytes))
      return Status::bad_digest;
    if (LawProfile::parse(bytes,draft.laws).code != Status::ok || !draft.laws.matches(configuration))
      return Status::law_mismatch;
    if (!pinned_file(trusted_root, draft.manifest.obs_file, draft.manifest.obs_sha256,bytes))
      return Status::bad_digest;
    if (Dataset::parse(bytes,draft.dataset).code != Status::ok) return Status::bad_dataset;
    if (draft.dataset.config_hash != configuration.hash ||
        draft.dataset.source_hash != configuration.material_sha)
      return Status::config_mismatch;
    output = std::move(draft);
    return Status::ok;
  }
};

inline bool predicted(const sim::Snapshot& snapshot, const Observation& row,
                      std::int64_t& amount) noexcept {
  amount = 0;
  if (row.colony != "all") {
    bool known = false;
    for (const auto& c : snapshot.colonies)
      if (c.id == row.colony) known = true;
    if (!known) return false;
  }
  for (const auto& c : snapshot.cells) {
    if (row.colony != "all" && c.colony != row.colony) continue;
    if (row.metric == "alive" && c.alive) ++amount;
    else if (row.metric == "energy") amount += c.energy;
    else if (row.metric == "learning") amount += c.learning;
    else if (row.metric == "maturity") amount += c.maturity;
  }
  for (const auto& c : snapshot.colonies) {
    if (row.colony != "all" && c.id != row.colony) continue;
    if (row.metric == "food") amount += c.food;
    else if (row.metric == "stress") amount += c.stress;
  }
  return true;
}
struct RowResult { Observation row; std::int64_t prediction=0,residual=0; bool within_declared=false; };
struct Partition {
  std::size_t count = 0;
  double mae=0,rmse=0,bias=0,declared_coverage=0,training_band_coverage=0;
};
struct Report {
  std::string json, sha256;
  std::size_t training_count=0, holdout_count=0;
  std::int64_t training_p90_band=0;
};
inline Partition aggregate(const std::vector<RowResult>& rows,std::string_view split,
                           std::int64_t training_band, std::string_view metric = {}) {
  Partition out;
  long double total_abs=0,total_squared=0,total_signed=0;
  std::size_t declared=0,calibrated=0;
  for (const auto& r : rows) {
    if (r.row.split != split || (!metric.empty() && r.row.metric != metric)) continue;
    ++out.count;
    const auto delta=std::llabs(r.residual);
    total_abs += static_cast<long double>(delta);
    total_squared += static_cast<long double>(delta) * static_cast<long double>(delta);
    total_signed += static_cast<long double>(r.residual);
    if (r.within_declared) ++declared;
    if (delta <= training_band) ++calibrated;
  }
  if (!out.count) return out;
  const auto divisor=static_cast<long double>(out.count);
  out.mae=static_cast<double>(total_abs/divisor);
  out.rmse=std::sqrt(static_cast<double>(total_squared/divisor));
  out.bias=static_cast<double>(total_signed/divisor);
  out.declared_coverage=static_cast<double>(static_cast<long double>(declared)/divisor);
  out.training_band_coverage=static_cast<double>(static_cast<long double>(calibrated)/divisor);
  return out;
}
inline std::string partition_json(const Partition& p){
  std::ostringstream out;
  out<<std::fixed<<std::setprecision(6)
     <<"{\"count\":"<<p.count<<",\"mae\":"<<p.mae<<",\"rmse\":"<<p.rmse
     <<",\"bias\":"<<p.bias<<",\"declared_band_coverage\":"<<p.declared_coverage
     <<",\"training_p90_band_coverage\":"<<p.training_band_coverage<<'}';
  return out.str();
}
inline Status evaluate(const sim::Configuration& cfg,const fs::path& history_root,
                       const Bundle& bundle,Report& output) {
  if (cfg.hash != bundle.manifest.config_hash || !bundle.laws.matches(cfg) ||
      bundle.dataset.config_hash != cfg.hash) return Status::config_mismatch;
  const auto& rows=bundle.dataset.rows;
  if (rows.empty()) return Status::bad_dataset;
  std::uint32_t max_tick=0;
  for (const auto& r : rows) max_tick=std::max(max_tick,r.tick);
  if (max_tick > kMaxEvaluationTick) return Status::bad_dataset;
  std::map<std::uint32_t, sim::Snapshot> snapshots;
  std::set<std::uint32_t> selected_ticks;
  for (const auto& r : rows) selected_ticks.insert(r.tick);
  for (std::uint32_t t=0;t<=max_tick;++t) {
    std::ostringstream name;
    name<<"tick-"<<std::setw(6)<<std::setfill('0')<<t<<".state";
    std::string bytes;
    const auto path=history_root/"history"/name.str();
    if (!sim::bounded_read(path,sim::kMaxSnapshotBytes,bytes)) return Status::missing_checkpoint;
    sim::Snapshot parsed;
    if (sim::decode_snapshot(bytes,cfg,parsed)!=sim::Result::ok || parsed.tick != t)
      return Status::bad_snapshot;
    if (selected_ticks.contains(t)) snapshots.emplace(t,std::move(parsed));
  }
  std::vector<RowResult> results;
  results.reserve(rows.size());
  std::vector<std::int64_t> training_abs;
  for (const auto& r : rows) {
    std::int64_t predicted_value=0;
    if (!predicted(snapshots.at(r.tick),r,predicted_value)) return Status::bad_dataset;
    const std::int64_t residual=predicted_value-r.measured;
    results.push_back({r,predicted_value,residual,std::llabs(residual)<=r.radius});
    if (r.split=="train") training_abs.push_back(std::llabs(residual));
  }
  std::sort(training_abs.begin(),training_abs.end());
  const auto p90_index=(training_abs.size()*9+9)/10-1; // ceil(0.9*N)-1
  const std::int64_t p90_band=training_abs.at(p90_index);
  const auto train=aggregate(results,"train",p90_band);
  const auto holdout=aggregate(results,"holdout",p90_band);
  std::ostringstream out;
  out<<"{\"schema\":\"CEVFIDELITY/1\",\"classification\":\"SYNTHETIC_ONLY_NOT_BIOLOGICAL_VALIDATION\","
     <<"\"independent_holdout_verified\":false,\"uncertainty_interpretation\":\"DECLARED_ABSOLUTE_PROXY_BAND\","
     <<"\"manifest_sha256\":\""<<bundle.manifest.sha256<<"\",\"config_sha256\":\""<<cfg.hash
     <<"\",\"source_sha256\":\""<<cfg.material_sha<<"\",\"law_sha256\":\""<<bundle.laws.sha256
     <<"\",\"dataset_sha256\":\""<<bundle.dataset.sha256<<"\",\"model_version\":"<<bundle.laws.version
     <<",\"training_p90_abs_error_band\":"<<p90_band<<",\"train\":"<<partition_json(train)
     <<",\"holdout\":"<<partition_json(holdout)<<",\"per_metric\":{";
  std::set<std::string> metrics;
  for (const auto& r : results) metrics.insert(r.row.metric);
  bool first_metric=true;
  for (const auto& metric : metrics) {
    std::vector<std::int64_t> errors;
    for (const auto& r : results)
      if (r.row.split == "train" && r.row.metric == metric)
        errors.push_back(std::llabs(r.residual));
    std::sort(errors.begin(),errors.end());
    const auto idx=(errors.size()*9+9)/10-1;
    const auto band=errors.at(idx);
    if (!first_metric) out<<',';
    first_metric=false;
    out<<'"'<<metric<<"\":{";
    out<<"\"training_band\":"<<band<<",\"train\":"
       <<partition_json(aggregate(results,"train",band,metric))
       <<",\"holdout\":"<<partition_json(aggregate(results,"holdout",band,metric))<<'}';
  }
  out<<"},\"rows\":[";
  for (std::size_t i=0;i<results.size();++i) {
    if (i) out<<',';
    const auto& r=results[i];
    out<<"{\"tick\":"<<r.row.tick<<",\"colony\":\""<<r.row.colony
       <<"\",\"metric\":\""<<r.row.metric<<"\",\"split\":\""<<r.row.split
       <<"\",\"observed\":"<<r.row.measured<<",\"predicted\":"<<r.prediction
       <<",\"residual\":"<<r.residual<<",\"declared_radius\":"<<r.row.radius
       <<",\"within_declared_band\":"<<(r.within_declared?"true":"false")<<'}';
  }
  out<<"]}\n";
  Report report;
  report.json=out.str();report.sha256=pl::sha256_hex(report.json);
  report.training_count=train.count;report.holdout_count=holdout.count;
  report.training_p90_band=p90_band;
  output=std::move(report);
  return Status::ok;
}
} // namespace christeye::living::fidelity
