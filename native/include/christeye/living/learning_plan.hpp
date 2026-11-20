#pragma once
// CEVPLAN/1: content-addressed, DIGITAL-ONLY learning-stage descriptors for
// CEVLM/2. Immutable input artifacts are verified, never executed as plugins.
// RNA labels and molecular support remain simulated/read-only evidence links.
#include "christeye/living/plan_hash.hpp"
#include "christeye/living/process_machine.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace christeye::living::plans {
namespace p = christeye::living::process;
namespace v = christeye::living::vlm;

inline constexpr std::size_t kMaxPlanBytes = 128 * 1024;
inline constexpr std::size_t kMaxArtifactBytes = 1024 * 1024;
inline constexpr std::size_t kMaxArtifacts = 128;
inline constexpr std::size_t kMaxStages = 64;

struct Artifact {
  std::string id, role, relative_path, sha256;
};
struct Binding {
  std::string id, crystal_id, learning_id, rna_id, support_id;
};
struct Stage {
  std::string id, process_id, binding_id, visual_id;
  std::uint32_t task_id = 0, duration = 0, weight = 0;
  v::Action action{};
};
struct Progress {
  std::string stage_id, crystal_hash, learning_hash, rna_hash, support_hash, visual_hash;
  std::string plan_hash, base_hash, receipt_hash;
  std::uint32_t task_id = 0, elapsed = 0, duration = 0, weight = 0;
  std::uint64_t tick = 0;
  std::int32_t x = 0, y = 0;
  bool completed = false;
  std::uint64_t machine_state = 0;
  std::int64_t support_field_proxy = 0;
  std::int64_t rna_field_proxy = 0;
  std::int64_t learning_proxy = 0;
  std::int64_t neurite_proxy = 0;
};
inline void write_progress_json(std::ostream& out,const Progress& e){
  out << "{\"origin\":\"SIMULATED_UNCALIBRATED\",\"event\":\"CE.PLAN.PROGRESS/1\""
      << ",\"stage\":\"" << e.stage_id << "\",\"task_id\":" << e.task_id
      << ",\"tick\":" << e.tick << ",\"elapsed\":" << e.elapsed
      << ",\"duration\":" << e.duration << ",\"weight\":" << e.weight
      << ",\"x\":" << e.x << ",\"y\":" << e.y
      << ",\"completed\":" << (e.completed ? "true":"false")
      << ",\"state_hash\":\"" << v::hex64(e.machine_state) << "\""
      << ",\"receipt_sha256\":\"" << e.receipt_hash << "\""
      << ",\"plan_sha256\":\"" << e.plan_hash << "\""
      << ",\"base_sha256\":\"" << e.base_hash << "\""
      << ",\"crystal_sha256\":\"" << e.crystal_hash << "\""
      << ",\"learning_sha256\":\"" << e.learning_hash << "\""
      << ",\"rna_sha256\":\"" << e.rna_hash << "\""
      << ",\"support_sha256\":\"" << e.support_hash << "\""
      << ",\"visual_sha256\":\"" << e.visual_hash << "\""
      << ",\"support_field_proxy\":" << e.support_field_proxy
      << ",\"rna_field_proxy\":" << e.rna_field_proxy
      << ",\"learning_proxy\":" << e.learning_proxy
      << ",\"neurite_proxy\":" << e.neurite_proxy << "}\n";
}

enum class Error : std::uint8_t {
  ok, malformed, duplicate_id, capacity, missing_reference, bad_role,
  invalid_path, asset_missing, asset_hash_mismatch, plan_hash_mismatch,
  base_mismatch, process_mismatch, stage_failure, finished, io_error
};
struct LoadResult { Error error = Error::ok; std::size_t line = 0; };

inline std::string_view role_header(std::string_view role) noexcept {
  if (role == "digital_crystal" || role == "physical_crystal_candidate") return "CECRYSTAL/1\n";
  if (role == "digital_learning") return "CELEARN/1\n";
  if (role == "rna_observation") return "CERNA/1\n";
  if (role == "support_profile") return "CESUPPORT/1\n";
  return {};
}
inline bool allowed_role(std::string_view role) {
  return role == "digital_crystal" || role == "physical_crystal_candidate" ||
         role == "digital_learning" || role == "rna_observation" ||
         role == "support_profile" || role == "visual";
}
inline bool valid_file_path(std::string_view value) {
  if (value.empty() || value.size() > 240 || value.front()=='/') return false;
  std::filesystem::path p{value};
  if (p.is_absolute() || p.has_root_name()) return false;
  for (const auto& part : p) {
    if (part == ".." || part == "." || part.empty()) return false;
  }
  for (char c : value) {
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' ||
          c == '/' || c == '.')) return false;
  }
  return true;
}
inline bool read_bounded(const std::filesystem::path& file, std::size_t limit,
                         std::string& out) {
  std::error_code ec;
  const auto n = std::filesystem::file_size(file, ec);
  if (ec || n > limit) return false;
  std::ifstream stream(file, std::ios::binary);
  if (!stream) return false;
  out.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
  return !stream.bad() && out.size() == n;
}
// Symlinks cannot escape the trusted local artifact root. No URL/network fetch.
inline bool rooted_file(const std::filesystem::path& root, std::string_view relative,
                        std::filesystem::path& result) {
  if (!valid_file_path(relative)) return false;
  std::error_code ec;
  const auto base = std::filesystem::canonical(root, ec);
  if (ec) return false;
  const auto target = std::filesystem::canonical(base / std::string(relative), ec);
  if (ec) return false;
  const auto sub = target.lexically_relative(base);
  if (sub.empty() || sub.is_absolute()) return false;
  for (const auto& part : sub) if (part == "..") return false;
  result = target;
  return true;
}

// Retained immutable inputs for execution after the original filesystem tree is gone.
struct ArtifactBytes { std::string relative_path, bytes; };

class Plan {
  std::string name_, base_sha_;
  std::vector<Artifact> artifacts_{};
  std::vector<Binding> bindings_{};
  std::vector<Stage> stages_{};
public:
  const std::string& name() const noexcept { return name_; }
  const std::string& base_sha() const noexcept { return base_sha_; }
  const std::vector<Artifact>& artifacts() const noexcept { return artifacts_; }
  const std::vector<Binding>& bindings() const noexcept { return bindings_; }
  const std::vector<Stage>& stages() const noexcept { return stages_; }
  const Artifact* artifact(std::string_view id) const noexcept {
    for (const auto& item : artifacts_) if (item.id == id) return &item;
    return nullptr;
  }
  const Binding* binding(std::string_view id) const noexcept {
    for (const auto& item : bindings_) if (item.id == id) return &item;
    return nullptr;
  }
  std::string serialize(bool seal = true) const {
    std::ostringstream out;
    out << "CEVPLAN/1\nname " << name_ << "\nbase_sha256 " << base_sha_ << "\nartifacts\n";
    for (const auto& a : artifacts_)
      out << "artifact " << a.id << ' ' << a.role << ' ' << a.relative_path << ' ' << a.sha256 << '\n';
    out << "endartifacts\nbindings\n";
    for (const auto& b : bindings_)
      out << "binding " << b.id << ' ' << b.crystal_id << ' ' << b.learning_id
          << ' ' << b.rna_id << ' ' << b.support_id << '\n';
    out << "endbindings\nstages\n";
    for (const auto& s : stages_)
      out << "stage " << s.id << ' ' << s.task_id << ' ' << s.process_id << ' '
          << s.binding_id << ' ' << s.visual_id << ' ' << s.duration << ' ' << s.weight
          << ' ' << v::serialize_action(s.action) << '\n';
    out << "endstages\n";
    if (seal) {
      const auto text=out.str();
      out << "seal " << sha256_hex(text) << '\n';
    }
    return out.str();
  }
  std::string hash() const { return sha256_hex(serialize(false)); }
  Error validate() const {
    if (!p::symbol(name_) || !p::valid_digest(base_sha_) || artifacts_.size() > kMaxArtifacts ||
        stages_.empty() || stages_.size() > kMaxStages || bindings_.size() != stages_.size())
      return Error::malformed;
    std::set<std::string> names;
    for (const auto& a : artifacts_) {
      if (!p::symbol(a.id) || !allowed_role(a.role) ||
          !valid_file_path(a.relative_path) || !p::valid_digest(a.sha256))
        return Error::malformed;
      if (!names.insert(a.id).second) return Error::duplicate_id;
    }
    names.clear();
    for (const auto& b : bindings_) {
      if (!p::symbol(b.id) || !names.insert(b.id).second) return Error::duplicate_id;
      for (const auto& [id, role] : {std::pair{b.learning_id, "digital_learning"},
                                    {b.rna_id, "rna_observation"},
                                    {b.support_id, "support_profile"}}) {
        const auto* a=artifact(id);
        if (!a) return Error::missing_reference;
        if (a->role != role) return Error::bad_role;
      }
      const auto* crystal=artifact(b.crystal_id);
      if (!crystal) return Error::missing_reference;
      if (!(crystal->role=="digital_crystal" || crystal->role=="physical_crystal_candidate"))
        return Error::bad_role;
    }
    names.clear();
    std::set<std::uint32_t> task_ids;
    std::set<std::string> used_bindings;
    for (const auto& s : stages_) {
      if (!p::symbol(s.id) || !p::symbol(s.process_id) ||
          s.task_id == 0 || !names.insert(s.id).second || !task_ids.insert(s.task_id).second)
        return Error::duplicate_id;
      if (!binding(s.binding_id)) return Error::missing_reference;
      if (!used_bindings.insert(s.binding_id).second) return Error::duplicate_id;
      const auto* visual=artifact(s.visual_id);
      if (!visual) return Error::missing_reference;
      if (visual->role != "visual") return Error::bad_role;
      if (s.duration < 1 || s.duration > 512 || s.weight < 1 || s.weight > 1000 ||
          (!s.action.is_deposit && s.action.instruction.op == Op::advance))
        return Error::malformed;
    }
    if (used_bindings.size()!=bindings_.size()) return Error::missing_reference;
    return Error::ok;
  }
  Error verify_assets(const std::vector<ArtifactBytes>& assets) const {
    if (assets.size()!=artifacts_.size()) return Error::asset_missing;
    std::set<std::string> seen;
    for (const auto& retained : assets) {
      if (!valid_file_path(retained.relative_path) ||
          !seen.insert(retained.relative_path).second) return Error::invalid_path;
      if (retained.bytes.size()>kMaxArtifactBytes) return Error::capacity;
    }
    for (const auto& a : artifacts_) {
      const ArtifactBytes* retained=nullptr;
      for (const auto& candidate : assets)
        if (candidate.relative_path==a.relative_path) retained=&candidate;
      if (!retained) return Error::asset_missing;
      if (sha256_hex(retained->bytes)!=a.sha256) return Error::asset_hash_mismatch;
      const auto header=role_header(a.role);
      if (!header.empty() && !std::string_view(retained->bytes).starts_with(header))
        return Error::bad_role;
    }
    return Error::ok;
  }
  Error verify_assets(const std::filesystem::path& root) const {
    for (const auto& a : artifacts_) {
      std::filesystem::path path;
      if (!rooted_file(root, a.relative_path, path)) return Error::invalid_path;
      std::string data;
      if (!read_bounded(path, kMaxArtifactBytes, data)) return Error::asset_missing;
      if (sha256_hex(data) != a.sha256) return Error::asset_hash_mismatch;
      const auto expected=role_header(a.role);
      if (!expected.empty() && !std::string_view(data).starts_with(expected))
        return Error::bad_role;
    }
    return Error::ok;
  }
  static LoadResult parse(std::string_view source, Plan& destination,
                          bool seal_required = true) {
    if (source.size()>kMaxPlanBytes) return {Error::capacity,0};
    Plan next;
    enum class Phase { magic, name, base, assets_open, assets, bindings_open,
                       bindings, stages_open, stages, seal, done };
    Phase phase=Phase::magic;
    bool saw_seal=false;
    std::istringstream in{std::string(source)};
    std::string line;
    std::size_t line_no=0;
    while (std::getline(in,line)) {
      ++line_no;
      if (!line.empty() && line.back()=='\r') line.pop_back();
      if (line.size()>512) return {Error::malformed,line_no};
      if (line.empty() || line.front()=='#') continue;
      std::vector<std::string> t;
      if (!p::split(line,t,24)) return {Error::malformed,line_no};
      if (phase==Phase::magic) {
        if (line!="CEVPLAN/1") return {Error::malformed,line_no};
        phase=Phase::name;
      } else if (phase==Phase::name) {
        if (t.size()!=2 || t[0]!="name") return {Error::malformed,line_no};
        next.name_=t[1];phase=Phase::base;
      } else if (phase==Phase::base) {
        if (t.size()!=2 || t[0]!="base_sha256") return {Error::malformed,line_no};
        next.base_sha_=t[1];phase=Phase::assets_open;
      } else if (phase==Phase::assets_open) {
        if (line!="artifacts") return {Error::malformed,line_no};
        phase=Phase::assets;
      } else if (phase==Phase::assets) {
        if (line=="endartifacts") {phase=Phase::bindings_open;continue;}
        if (t.size()!=5 || t[0]!="artifact" || next.artifacts_.size()>=kMaxArtifacts)
          return {Error::malformed,line_no};
        next.artifacts_.push_back({t[1],t[2],t[3],t[4]});
      } else if (phase==Phase::bindings_open) {
        if (line!="bindings") return {Error::malformed,line_no};
        phase=Phase::bindings;
      } else if (phase==Phase::bindings) {
        if (line=="endbindings") {phase=Phase::stages_open;continue;}
        if (t.size()!=6 || t[0]!="binding" || next.bindings_.size()>=kMaxStages)
          return {Error::malformed,line_no};
        next.bindings_.push_back({t[1],t[2],t[3],t[4],t[5]});
      } else if (phase==Phase::stages_open) {
        if (line!="stages") return {Error::malformed,line_no};
        phase=Phase::stages;
      } else if (phase==Phase::stages) {
        if (line=="endstages") {phase=Phase::seal;continue;}
        if (t.size()<10 || t[0]!="stage" || next.stages_.size()>=kMaxStages)
          return {Error::malformed,line_no};
        Stage s{};
        s.id=t[1];s.process_id=t[3];s.binding_id=t[4];s.visual_id=t[5];
        if (!v::parse_unsigned(t[2],s.task_id) ||
            !v::parse_unsigned(t[6],s.duration) ||
            !v::parse_unsigned(t[7],s.weight)) return {Error::malformed,line_no};
        std::string action;
        for(std::size_t i=8;i<t.size();++i) {
          if(i!=8)action+=' ';
          action+=t[i];
        }
        if (!v::parse_action(action,s.action)) return {Error::malformed,line_no};
        next.stages_.push_back(std::move(s));
      } else if (phase==Phase::seal) {
        if (t.size()!=2 || t[0]!="seal" || !p::valid_digest(t[1]))
          return {Error::malformed,line_no};
        if (t[1]!=next.hash()) return {Error::plan_hash_mismatch,line_no};
        saw_seal=true;phase=Phase::done;
      } else return {Error::malformed,line_no};
    }
    if (!(phase==Phase::seal || phase==Phase::done) || (seal_required && !saw_seal))
      return {Error::malformed,line_no};
    auto e=next.validate();
    if (e!=Error::ok) return {e,line_no};
    destination=std::move(next);
    return {};
  }
};

inline LoadResult read_plan(const std::filesystem::path& path, Plan& destination,
                            bool require_seal = true) {
  std::string text;
  if (!read_bounded(path,kMaxPlanBytes,text)) return {Error::io_error,0};
  return Plan::parse(text,destination,require_seal);
}
inline std::string machine_sha256(const p::ProcessMachine& machine) {
  return sha256_hex(machine.serialize(true));
}

// A PlanSession is explicitly stepped by a simulated tick source. It does NOT
// use timer polling, real-world molecular actuation, or physical stimulation.
// Each callback follows a committed VLM tick; callback is non-reentrant.
class Session {
  const Plan* plan_ = nullptr;
  p::ProcessMachine machine_{};
  std::string base_hash_{};
  std::size_t position_ = 0;
  bool active_ = false;
  using Handler = void (*)(void*, const Progress&) noexcept;
  Handler callback_ = nullptr;
  void* callback_context_ = nullptr;
  std::vector<Progress> progress_{};
public:
  const p::ProcessMachine& machine() const noexcept { return machine_; }
  const std::vector<Progress>& progress() const noexcept { return progress_; }
  bool finished() const noexcept {
    return plan_ != nullptr && position_ == plan_->stages().size();
  }
  void subscribe(Handler f, void* context) noexcept { callback_=f;callback_context_=context; }
  Error initialize(const Plan& plan, const p::ProcessMachine& base,
                   const std::filesystem::path& asset_root) {
    return initialize_checked(plan,base,plan.verify_assets(asset_root));
  }
  Error initialize(const Plan& plan, const p::ProcessMachine& base,
                   const std::vector<ArtifactBytes>& assets) {
    return initialize_checked(plan,base,plan.verify_assets(assets));
  }
private:
  Error initialize_checked(const Plan& plan, const p::ProcessMachine& base,
                           Error assets) {
    // A failed re-enrolment must not leave a stale, executable previous plan.
    plan_=nullptr; active_=false; position_=0; progress_.clear();
    if (auto r=plan.validate();r!=Error::ok) return r;
    if (machine_sha256(base)!=plan.base_sha()) return Error::base_mismatch;
    if (assets!=Error::ok) return assets;
    for (const auto& stage : plan.stages()) {
      const p::Definition* def=nullptr;
      for (const auto& item : base.definitions())
        if (item.id==stage.process_id) def=&item;
      if (!def || def->duration!=stage.duration || def->weight!=stage.weight)
        return Error::process_mismatch;
      const auto action_text=v::serialize_action(stage.action);
      const auto space=action_text.find(' ');
      const auto verb=action_text.substr(0,space);
      if (verb!=def->operation) return Error::process_mismatch;
      for (const auto& task : base.tasks())
        if (task.id==stage.task_id) return Error::process_mismatch;
      const auto* vasset=plan.artifact(stage.visual_id);
      const auto* linked_asset=base.asset(def->asset_id);
      if (!vasset || !linked_asset || vasset->relative_path!=linked_asset->path ||
          vasset->sha256!=linked_asset->sha256)
        return Error::process_mismatch;
      // Real material candidates are provenance-only and cannot be executable
      // transfer packages; this surrogate cannot authorize physical delivery.
      if (stage.action.is_deposit && stage.action.amount<0) return Error::stage_failure;
    }
    machine_=base;
    base_hash_=plan.base_sha();
    plan_=&plan;
    position_=0;active_=false;progress_.clear();
    return Error::ok;
  }
public:
  Error step() {
    if (!plan_) return Error::malformed;
    if (finished()) return Error::finished;
    const auto& stage=plan_->stages()[position_];
    if (!active_) {
      const auto r=machine_.start(stage.task_id,stage.process_id,stage.action);
      if (r!=p::Error::ok) return Error::stage_failure;
      active_=true;
    }
    const auto prev=machine_.events().size();
    if (machine_.advance(1)!=p::Error::ok) return Error::stage_failure;
    const p::Event* event=nullptr;
    for (std::size_t i=prev;i<machine_.events().size();++i) {
      if (machine_.events()[i].task_id==stage.task_id) event=&machine_.events()[i];
    }
    if (!event) return Error::stage_failure;
    const auto* b=plan_->binding(stage.binding_id);
    if (!b) return Error::missing_reference;
    const auto* crystal=plan_->artifact(b->crystal_id);
    const auto* lesson=plan_->artifact(b->learning_id);
    const auto* rna=plan_->artifact(b->rna_id);
    const auto* support=plan_->artifact(b->support_id);
    const auto* visual=plan_->artifact(stage.visual_id);
    Progress rec{stage.id,crystal->sha256,lesson->sha256,rna->sha256,support->sha256,
                 visual->sha256,plan_->hash(),base_hash_,{},stage.task_id,
                 event->elapsed,stage.duration,stage.weight,event->tick,event->x,event->y,
                 event->completed,machine_.state_hash()};
    std::ostringstream receipt;
    receipt << "CE/PLAN/PROGRESS/1\n" << rec.plan_hash << '\n' << rec.base_hash << '\n'
            << rec.stage_id << '\n' << rec.task_id << ' ' << rec.tick << ' ' << rec.elapsed
            << ' ' << rec.machine_state << '\n' << rec.crystal_hash << rec.learning_hash
            << rec.rna_hash << rec.support_hash << rec.visual_hash << '\n';
    rec.support_field_proxy=machine_.base().soil().total(v::FieldKind::support);
    rec.rna_field_proxy=machine_.base().soil().total(v::FieldKind::rna);
    const auto metrics=machine_.base().core().metrics();
    rec.learning_proxy=metrics.learning_proxy;
    rec.neurite_proxy=metrics.neurite_proxy;
    receipt << rec.support_field_proxy << ' ' << rec.rna_field_proxy << ' '
            << rec.learning_proxy << ' ' << rec.neurite_proxy << '\n';
    rec.receipt_hash=sha256_hex(receipt.str());
    progress_.push_back(rec);
    if (event->completed) {++position_;active_=false;}
    if (callback_) callback_(callback_context_,progress_.back());
    return Error::ok;
  }
};

// Reverse index from the saved CEVLM/2 output back to the immutable plan,
// base, per-stage bindings and receipt stream. Not a certificate of learning.
inline std::string mapping_index(const Plan& plan, const p::ProcessMachine& output,
                                 std::string_view receipt_ndjson) {
  std::ostringstream text;
  text << "CEVMAP/1\nbase_sha256 " << plan.base_sha()
       << "\nplan_sha256 " << plan.hash()
       << "\noutput_sha256 " << machine_sha256(output)
       << "\nreceipts_sha256 " << sha256_hex(receipt_ndjson) << "\nbindings\n";
  for (const auto& stage : plan.stages()) {
    const auto* b=plan.binding(stage.binding_id);
    if (!b) continue;  // plan.validate() is required before this helper
    const auto* crystal=plan.artifact(b->crystal_id);
    const auto* learning=plan.artifact(b->learning_id);
    const auto* rna=plan.artifact(b->rna_id);
    const auto* support=plan.artifact(b->support_id);
    const auto* visual=plan.artifact(stage.visual_id);
    if (!crystal || !learning || !rna || !support || !visual) continue;
    text << "binding " << stage.id << ' ' << b->id << ' '
         << crystal->sha256 << ' ' << learning->sha256 << ' '
         << rna->sha256 << ' ' << support->sha256 << ' '
         << visual->sha256 << '\n';
  }
  text << "endbindings\n";
  const auto canonical=text.str();
  text << "seal " << sha256_hex(canonical) << '\n';
  return text.str();
}
} // namespace christeye::living::plans
