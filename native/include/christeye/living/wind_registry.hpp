#pragma once
// CE-SW-10: single-process, host-side content-addressed evidence registry.
// Simulated / imported research records only. No biological actuation, network
// transport, authentication, fsync guarantees, or laboratory qualification.
#include "christeye/living/learning_plan.hpp"
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace christeye::living::registry {
namespace fs = std::filesystem;
namespace pl = christeye::living::plans;
inline constexpr std::size_t kMaxRegistryObjectBytes = 4 * 1024 * 1024;
inline constexpr std::size_t kMaxRegistryJournalBytes = 4 * 1024 * 1024;
inline constexpr std::size_t kMaxRegistryEvents = 4096;
inline constexpr std::string_view kZeroDigest =
    "0000000000000000000000000000000000000000000000000000000000000000";

enum class Code : std::uint8_t {
  ok, io_error, bad_argument, missing_object, bad_digest, corrupt_journal,
  capacity, plan_rejected, duplicate_event, not_open
};
struct Event {
  std::uint64_t sequence = 0;
  std::string previous_hash;
  std::string kind;
  std::string entity;
  std::string object_hash;
  std::string origin;
  std::uint64_t source_tick = 0;
  std::string clock;
  std::string producer; // provenance label, not authenticated identity
  std::string hash;
};
struct StoreResult { Code status = Code::ok; std::string hash; };

inline bool valid_id(std::string_view value) noexcept {
  if (value.empty() || value.size() > 80) return false;
  for (auto c : value)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.')) return false;
  return true;
}
inline bool valid_kind(std::string_view value) noexcept {
  return value == "PLAN_REGISTERED" || value == "RNA_OBSERVATION" ||
         value == "LEARNING_RESULT" || value == "SUPPORT_OBSERVATION" ||
         value == "CRYSTAL_EVIDENCE" || value == "SUCCESSOR_PROPOSED";
}
inline bool valid_origin(std::string_view value) noexcept {
  return value == "SIMULATED" || value == "IMPORTED_UNVERIFIED" ||
         value == "INFERRED";
}
inline bool valid_digest(std::string_view value) noexcept {
  if (value.size() != 64) return false;
  for (char c : value) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
  return true;
}
inline bool parse_u64(std::string_view src, std::uint64_t& dst) noexcept {
  const auto r = std::from_chars(src.data(), src.data() + src.size(), dst);
  return r.ec == std::errc{} && r.ptr == src.data() + src.size();
}
inline std::string event_core(const Event& e) {
  std::ostringstream out;
  out << "event " << e.sequence << ' ' << e.previous_hash << ' ' << e.kind << ' '
      << e.entity << ' ' << e.object_hash << ' ' << e.origin << ' '
      << e.source_tick << ' ' << e.clock << ' ' << e.producer;
  return out.str();
}
inline std::string event_digest(const Event& e) {
  return pl::sha256_hex(std::string("CE/WINDREG/EVENT/1\n") + event_core(e) + '\n');
}
inline std::string serialize_event(const Event& e) {
  return event_core(e) + ' ' + e.hash + '\n';
}
inline bool parse_event(std::string_view line, Event& result) {
  std::istringstream input{std::string(line)};
  std::string magic, seq, tick, extra;
  Event next;
  if (!(input >> magic >> seq >> next.previous_hash >> next.kind >> next.entity
              >> next.object_hash >> next.origin >> tick >> next.clock >> next.producer >> next.hash) ||
      input >> extra || magic != "event" ||
      !parse_u64(seq, next.sequence) || !parse_u64(tick, next.source_tick) ||
      !valid_digest(next.previous_hash) || !valid_digest(next.object_hash) ||
      !valid_digest(next.hash) || !valid_kind(next.kind) ||
      !valid_origin(next.origin) || !valid_id(next.entity) ||
      !valid_id(next.clock) || !valid_id(next.producer) || event_digest(next) != next.hash)
    return false;
  result = std::move(next);
  return true;
}

// The root is operator-controlled. This V1 store is one writer per root;
// multiple processes sharing a root require OS locking in a later release.
class Registry {
  fs::path root_;
  std::vector<Event> events_{};
  bool open_ = false;
  using Callback = void (*)(void*, const Event&) noexcept;
  struct Subscription { Callback callback; void* context; std::string entity; std::string kind; };
  std::vector<Subscription> subscribers_{};
  fs::path object_path(std::string_view hash) const {
    return root_ / "objects" / std::string(hash);
  }
  bool object_good(std::string_view hash) const {
    if (!valid_digest(hash)) return false;
    const auto path = object_path(hash);
    std::error_code ec;
    if (fs::is_symlink(path, ec) || ec) return false;
    std::string data;
    return pl::read_bounded(path, kMaxRegistryObjectBytes, data) &&
           pl::sha256_hex(data) == hash;
  }
public:
  Code open(const fs::path& root) {
    // Reopening never leaves a previously open registry active on failure.
    open_ = false; events_.clear(); subscribers_.clear(); root_ = root;
    std::error_code ec;
    // SW2.1 roots require the exclusive lock and committed HEAD; V1 must not
    // bypass it by opening/writing the same directory.
    if (fs::exists(root / "HEAD", ec) || ec) return Code::io_error;
    fs::create_directories(root / "objects", ec);
    if (ec) return Code::io_error;
    const fs::path journal = root / "events.log";
    if (!fs::exists(journal, ec)) {
      std::ofstream create(journal, std::ios::binary | std::ios::trunc);
      if (!create) return Code::io_error;
      create << "CEWREG/1\n";
      create.flush();
      if (!create) return Code::io_error;
    }
    if (fs::is_symlink(journal, ec) || ec) return Code::io_error;
    std::string data;
    if (!pl::read_bounded(journal, kMaxRegistryJournalBytes, data)) return Code::io_error;
    if (!data.starts_with("CEWREG/1\n") || data.empty() || data.back() != '\n')
      return Code::corrupt_journal;
    std::istringstream lines(data);
    std::string line;
    std::getline(lines, line); // version header
    std::string previous(kZeroDigest);
    std::uint64_t count = 0;
    std::vector<Event> staging;
    while (std::getline(lines, line)) {
      Event record;
      if (count >= kMaxRegistryEvents || !parse_event(line, record) ||
          record.sequence != count + 1 || record.previous_hash != previous ||
          !object_good(record.object_hash)) return Code::corrupt_journal;
      previous = record.hash;
      staging.push_back(std::move(record));
      ++count;
    }
    root_ = root;
    events_ = std::move(staging);
    open_ = true;
    return Code::ok;
  }
  // Bytes are never rewritten at an existing hash. Returns the verified SHA.
  StoreResult put(std::string_view content) {
    if (!open_) return {Code::not_open, {}};
    if (content.size() > kMaxRegistryObjectBytes) return {Code::capacity, {}};
    const std::string hash = pl::sha256_hex(content);
    const auto path = object_path(hash);
    std::error_code ec;
    if (fs::exists(path, ec))
      return object_good(hash) ? StoreResult{Code::ok, hash} : StoreResult{Code::bad_digest, {}};
    if (ec) return {Code::io_error, {}};
    const auto tmp = path.string() + ".tmp";
    {
      std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
      if (!out) return {Code::io_error, {}};
      out.write(content.data(), static_cast<std::streamsize>(content.size()));
      out.flush();
      if (!out) return {Code::io_error, {}};
    }
    fs::rename(tmp, path, ec);
    if (ec) { fs::remove(tmp); return {Code::io_error, {}}; }
    return object_good(hash) ? StoreResult{Code::ok, hash} : StoreResult{Code::bad_digest, {}};
  }
  StoreResult put_file(const fs::path& file, std::string_view expected) {
    if (!valid_digest(expected)) return {Code::bad_argument, {}};
    std::error_code ec;
    if (fs::is_symlink(file, ec) || ec) return {Code::bad_argument, {}};
    std::string data;
    if (!pl::read_bounded(file, kMaxRegistryObjectBytes, data)) return {Code::io_error, {}};
    if (pl::sha256_hex(data) != expected) return {Code::bad_digest, {}};
    return put(data);
  }
  bool get(std::string_view hash, std::string& result) const {
    if (!open_ || !object_good(hash)) return false;
    return pl::read_bounded(object_path(hash), kMaxRegistryObjectBytes, result);
  }
  Code subscribe(Callback cb, void* ctx, std::uint64_t after_sequence = 0,
                 std::string entity = {}, std::string kind = {}) {
    if (!open_) return Code::not_open;
    if (cb == nullptr || after_sequence > events_.size() ||
        (!entity.empty() && !valid_id(entity)) ||
        (!kind.empty() && !valid_kind(kind))) return Code::bad_argument;
    if (subscribers_.size() >= 32) return Code::capacity;
    // Replay first, register before new publishes; single owner, no reentrancy.
    // A production transport must authorize each remote subscription.
    for (const auto& e : events_) {
      if (e.sequence > after_sequence &&
          (entity.empty() || e.entity == entity) &&
          (kind.empty() || e.kind == kind)) cb(ctx, e);
    }
    subscribers_.push_back({cb, ctx, std::move(entity), std::move(kind)});
    return Code::ok;
  }
  std::vector<Event> inbox(std::string_view entity, std::uint64_t after_sequence = 0,
                           std::string_view kind = {}) const {
    std::vector<Event> found;
    if (!open_ || (!entity.empty() && !valid_id(entity)) ||
        (!kind.empty() && !valid_kind(kind))) return found;
    for (const auto& e : events_)
      if (e.sequence > after_sequence &&
          (entity.empty() || e.entity == entity) &&
          (kind.empty() || e.kind == kind)) found.push_back(e);
    return found;
  }
  StoreResult publish(std::string kind, std::string entity, std::string artifact_hash,
                      std::string origin, std::uint64_t source_tick, std::string clock,
                      std::string producer = "local-simulator") {
    if (!open_) return {Code::not_open, {}};
    if (!valid_kind(kind) || !valid_id(entity) || !valid_digest(artifact_hash) ||
        !valid_origin(origin) || !valid_id(clock) || !valid_id(producer))
      return {Code::bad_argument, {}};
    if (!object_good(artifact_hash)) return {Code::missing_object, {}};
    for (const auto& existing : events_) {
      if (existing.kind == kind && existing.entity == entity &&
          existing.object_hash == artifact_hash && existing.origin == origin &&
          existing.source_tick == source_tick && existing.clock == clock &&
          existing.producer == producer)
        return {Code::ok, existing.hash};  // idempotent retry: no duplicate push
    }
    if (events_.size() >= kMaxRegistryEvents) return {Code::capacity, {}};
    Event record{events_.size() + 1,
                 events_.empty() ? std::string(kZeroDigest) : events_.back().hash,
                 std::move(kind), std::move(entity), std::move(artifact_hash),
                 std::move(origin), source_tick, std::move(clock), std::move(producer), {}};
    record.hash = event_digest(record);
    // One writer; cannot claim fsync-level power-loss durability.
    std::ofstream out(root_ / "events.log", std::ios::binary | std::ios::app);
    if (!out) return {Code::io_error, {}};
    const auto line = serialize_event(record);
    out.write(line.data(), static_cast<std::streamsize>(line.size()));
    out.flush();
    if (!out) return {Code::io_error, {}};
    out.close();
    if (!out) return {Code::io_error, {}};
    events_.push_back(record);
    for (const auto& sub : subscribers_) {
      if ((sub.entity.empty() || sub.entity == events_.back().entity) &&
          (sub.kind.empty() || sub.kind == events_.back().kind))
        sub.callback(sub.context, events_.back());
    }
    return {Code::ok, events_.back().hash};
  }
  // Copy a checked plan and its referenced assets into the immutable store.
  // Input plan is not executed; only checked and archived.
  StoreResult register_plan(const pl::Plan& plan, const fs::path& trusted_root) {
    if (!open_) return {Code::not_open, {}};
    if (plan.validate() != pl::Error::ok ||
        plan.verify_assets(trusted_root) != pl::Error::ok)
      return {Code::plan_rejected, {}};
    for (const auto& artifact : plan.artifacts()) {
      fs::path path;
      if (!pl::rooted_file(trusted_root, artifact.relative_path, path))
        return {Code::plan_rejected, {}};
      if (put_file(path, artifact.sha256).status != Code::ok)
        return {Code::plan_rejected, {}};
    }
    const auto object = put(plan.serialize(true));
    if (object.status != Code::ok) return object;
    const auto r = publish("PLAN_REGISTERED", plan.name(), object.hash,
                           "SIMULATED", 0, "plan-revision");
    return r.status == Code::ok ? StoreResult{Code::ok, object.hash} : r;
  }
  const std::vector<Event>& events() const noexcept { return events_; }
  bool verify() const {
    if (!open_) return false;
    std::string previous(kZeroDigest);
    for (std::size_t i = 0; i < events_.size(); ++i) {
      const auto& e = events_[i];
      if (e.sequence != i + 1 || e.previous_hash != previous ||
          e.hash != event_digest(e) || !object_good(e.object_hash)) return false;
      previous = e.hash;
    }
    return true;
  }
};

// Explicit one-to-one stage -> binding -> original four evidence hashes.
// An RNA observation is a referenced record, not a biological state mutation.
struct StageMapping {
  std::string stage, plan, crystal, learning, rna, support, visual;
};
inline bool resolve_stage(const pl::Plan& plan, std::string_view stage_id,
                          StageMapping& result) {
  for (const auto& stage : plan.stages()) {
    if (stage.id != stage_id) continue;
    const auto* binding = plan.binding(stage.binding_id);
    if (!binding) return false;
    const auto* crystal = plan.artifact(binding->crystal_id);
    const auto* learning = plan.artifact(binding->learning_id);
    const auto* rna = plan.artifact(binding->rna_id);
    const auto* support = plan.artifact(binding->support_id);
    const auto* visual = plan.artifact(stage.visual_id);
    if (!crystal || !learning || !rna || !support || !visual) return false;
    result = {stage.id, plan.hash(), crystal->sha256, learning->sha256,
              rna->sha256, support->sha256, visual->sha256};
    return true;
  }
  return false;
}

// Immutable proposal for a future separately reviewed CEVPLAN/1 successor.
// It never edits the parent learning plan or declares physical Crystal transfer.
inline std::string successor_manifest(std::string_view parent_plan_object_sha,
                                      std::string_view learning_object_sha,
                                      std::string_view evidence_object_sha,
                                      std::string_view candidate_id) {
  if (!valid_digest(parent_plan_object_sha) || !valid_digest(learning_object_sha) ||
      !valid_digest(evidence_object_sha) || !valid_id(candidate_id)) return {};
  std::ostringstream out;
  out << "CEVSUCCESSOR/1\nparent_plan_object_sha256 " << parent_plan_object_sha
      << "\nlearning_object_sha256 " << learning_object_sha
      << "\nevidence_object_sha256 " << evidence_object_sha
      << "\ncandidate " << candidate_id
      << "\nstate PROPOSED_ONLY\n";
  const std::string core = out.str();
  out << "seal " << pl::sha256_hex(core) << '\n';
  return out.str();
}
}  // namespace christeye::living::registry
