#pragma once
// ChristEye Anaiero VLM2 — deterministic, DIGITAL-ONLY process/visual timeline.
// This is a simulator, NOT a kinetic biological model, culture protocol,
// molecular manufacturing system or physical stimulation controller.
// Assets are inert relative paths for authorized viewers; never loaded by this kernel.
#include "christeye/living/virtual_machine.hpp"
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace christeye::living::process {
namespace v = christeye::living::vlm;

inline constexpr std::size_t kMaxDefinitions = 64;
inline constexpr std::size_t kMaxAssets = 64;
inline constexpr std::size_t kMaxTasks = 128;
inline constexpr std::size_t kMaxEntries = 1024;
inline constexpr std::size_t kMaxEvents = 4096;

inline bool symbol(std::string_view s) noexcept {
  if (s.empty() || s.size() > 48) return false;
  for (char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
  return true;
}
inline bool asset_path(std::string_view s) noexcept {
  if (s.empty() || s.size() > 160 || s.front() == '/' || s.find("..") != s.npos)
    return false;
  for (char c : s)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '/' || c == '.'))
      return false;
  return s.ends_with(".svg") || s.ends_with(".png") || s.ends_with(".webp");
}
inline void hash_text(std::uint64_t& h, std::string_view s) noexcept {
  detail::hash_u64(h, s.size());
  for (char c : s) detail::hash_u64(h, static_cast<unsigned char>(c));
}
inline bool split(std::string_view s, std::vector<std::string>& out,
                  std::size_t max = 16) {
  std::istringstream stream{std::string(s)};
  std::string word;
  while (stream >> word) {
    if (word.size() > 160 || out.size() >= max) return false;
    out.push_back(std::move(word));
  }
  return !out.empty();
}

struct Asset {
  std::string id;
  std::string role; // icon | sprite | heatmap: viewer contract only
  std::string path; // relative URL in a separately authorized static viewer
};
struct Definition {
  std::string id;
  std::string operation; // spawn | molecule | deposit | ...
  std::uint32_t duration = 1; // synthetic logical ticks, not physical time
  std::uint32_t weight = 500; // event/scheduler priority 1..1000, not biological potency
  std::int32_t dx = 0, dy = 0; // synthetic motion per tick, grid units
  std::string asset_id;
};
struct Task {
  std::uint32_t id = 0;
  std::size_t definition = 0;
  v::Action action{};
  std::uint32_t elapsed = 0;
  std::int32_t x = 0, y = 0; // render/event cursor
  bool completed = false;
};
struct Event {
  std::uint64_t tick = 0;
  std::uint32_t task_id = 0;
  std::uint32_t elapsed = 0;
  std::int32_t x = 0, y = 0;
  std::uint32_t weight = 0;
  bool completed = false;
};
enum class Error : std::uint8_t {
  ok, malformed, invalid_state, unknown_asset, unknown_process,
  invalid_task, duplicate_id, capacity, execution_failed, checksum, io_error
};
struct LoadResult { Error error = Error::ok; std::size_t line = 0; };

// Saved journal entries represent commands, not per-tick derived states.
// On load we replay all commands; progress and events are reconstructed.
struct JournalEntry {
  bool start = false;
  std::uint32_t task_id = 0;
  std::string process_id;
  v::Action action{};
  std::int32_t advance = 0;
  bool direct = false; // journalled immediate VLM1 operation
};

class ProcessMachine {
  v::VirtualMachine base_{};
  std::vector<Asset> assets_{};
  std::vector<Definition> defs_{};
  std::vector<Task> tasks_{};
  std::vector<Event> events_{};
  std::vector<JournalEntry> journal_{};
  bool catalog_frozen_ = false;
  bool image_frozen_ = false;

  const Definition* find_def(std::string_view id) const noexcept {
    for (const auto& d : defs_) if (d.id == id) return &d;
    return nullptr;
  }
  bool has_asset(std::string_view id) const noexcept {
    for (const auto& a : assets_) if (a.id == id) return true;
    return false;
  }
  static std::string action_name(const v::Action& a) {
    if (a.is_deposit) return "deposit";
    if (a.instruction.op == Op::define_molecule) return "molecule";
    return v::serialize_action(a).substr(0, v::serialize_action(a).find(' '));
  }
  Error start_direct(std::uint32_t task_id, std::string_view process_id,
                     const v::Action& action) {
    if (!image_frozen_ || journal_.size() >= kMaxEntries || tasks_.size() >= kMaxTasks || task_id == 0)
      return Error::capacity;
    const auto* d = find_def(process_id);
    if (!d) return Error::unknown_process;
    if (d->operation != action_name(action)) return Error::invalid_task;
    for (const auto& t : tasks_) if (t.id == task_id) return Error::duplicate_id;
    std::size_t idx = static_cast<std::size_t>(d - defs_.data());
    Task t{};
    t.id = task_id; t.definition = idx; t.action = action;
    if (action.is_deposit) {
      t.x = static_cast<std::int32_t>(action.x);
      t.y = static_cast<std::int32_t>(action.y);
    } else if (action.instruction.op == Op::spawn) {
      t.x = action.instruction.value;
      t.y = action.instruction.extra;
    } else if (const auto* c = base_.core().cell(action.instruction.subject)) {
      t.x = c->x; t.y = c->y;
    }
    if (static_cast<std::int64_t>(t.x) + static_cast<std::int64_t>(d->dx) * d->duration < -4096 ||
        static_cast<std::int64_t>(t.x) + static_cast<std::int64_t>(d->dx) * d->duration > 4096 ||
        static_cast<std::int64_t>(t.y) + static_cast<std::int64_t>(d->dy) * d->duration < -4096 ||
        static_cast<std::int64_t>(t.y) + static_cast<std::int64_t>(d->dy) * d->duration > 4096)
      return Error::invalid_task;
    tasks_.push_back(t);
    journal_.push_back({true, task_id, std::string(process_id), action, 0});
    return Error::ok;
  }
  Error advance_direct(std::int32_t ticks) {
    if (!image_frozen_ || ticks < 1 || ticks > 512 || journal_.size() >= kMaxEntries)
      return Error::invalid_task;
    for (std::int32_t tick = 0; tick < ticks; ++tick) {
      if (base_.advance(1) != v::Result::ok) return Error::execution_failed;
      std::vector<std::size_t> active;
      for (std::size_t i = 0; i < tasks_.size(); ++i) if (!tasks_[i].completed) active.push_back(i);
      if (events_.size() + active.size() > kMaxEvents) return Error::capacity;
      // Higher weight acts first if multiple completed actions conflict.
      std::sort(active.begin(), active.end(), [&](std::size_t a, std::size_t b) {
        const auto& da = defs_[tasks_[a].definition];
        const auto& db = defs_[tasks_[b].definition];
        return da.weight != db.weight ? da.weight > db.weight : tasks_[a].id < tasks_[b].id;
      });
      for (auto idx : active) {
        auto& t = tasks_[idx];
        const auto& d = defs_[t.definition];
        ++t.elapsed;
        t.x += d.dx; t.y += d.dy;
        if (t.elapsed == d.duration) {
          auto action = t.action;
          if (action.is_deposit) {
            if (t.x < 0 || t.y < 0 || t.x >= static_cast<std::int32_t>(v::kFieldSide) ||
                t.y >= static_cast<std::int32_t>(v::kFieldSide)) return Error::execution_failed;
            action.x = static_cast<std::uint32_t>(t.x);
            action.y = static_cast<std::uint32_t>(t.y);
          } else if (action.instruction.op == Op::spawn) {
            if (t.x < 0 || t.y < 0 || t.x > kWorldMax || t.y > kWorldMax)
              return Error::execution_failed;
            action.instruction.value = t.x;
            action.instruction.extra = t.y;
          }
          if (base_.apply(action) != v::Result::ok) return Error::execution_failed;
          t.completed = true;
        }
        events_.push_back({base_.core().tick(), t.id, t.elapsed, t.x, t.y, d.weight, t.completed});
      }
    }
    journal_.push_back({false, 0, {}, {}, ticks});
    return Error::ok;
  }
  Error direct_action(const v::Action& action) {
    if (!image_frozen_ || journal_.size() >= kMaxEntries ||
        (!action.is_deposit && action.instruction.op == Op::advance)) return Error::invalid_task;
    if (base_.apply(action) != v::Result::ok) return Error::execution_failed;
    journal_.push_back({false, 0, {}, action, 0, true});
    return Error::ok;
  }
public:
  const v::VirtualMachine& base() const noexcept { return base_; }
  const std::vector<Task>& tasks() const noexcept { return tasks_; }
  const std::vector<Event>& events() const noexcept { return events_; }
  const std::vector<Definition>& definitions() const noexcept { return defs_; }
  const std::vector<Asset>& assets() const noexcept { return assets_; }
  const std::vector<JournalEntry>& journal() const noexcept { return journal_; }
  const Asset* asset(std::string_view id) const noexcept {
    for (const auto& a : assets_) if (a.id == id) return &a;
    return nullptr;
  }
  std::uint64_t tick() const noexcept { return base_.core().tick(); }
  bool name(std::string s) { return base_.set_name(std::move(s)); }

  Error add_asset(const Asset& a) {
    if (catalog_frozen_) return Error::invalid_state;
    if (assets_.size() >= kMaxAssets || !symbol(a.id) ||
        !(a.role == "icon" || a.role == "sprite" || a.role == "heatmap") ||
        !asset_path(a.path) || has_asset(a.id)) return Error::invalid_task;
    assets_.push_back(a); return Error::ok;
  }
  Error add_definition(const Definition& d) {
    if (catalog_frozen_) return Error::invalid_state;
    if (defs_.size() >= kMaxDefinitions || !symbol(d.id) || !has_asset(d.asset_id) ||
        d.duration < 1 || d.duration > 512 || d.weight < 1 || d.weight > 1000 ||
        d.dx < -8 || d.dx > 8 || d.dy < -8 || d.dy > 8) return Error::invalid_task;
    for (const auto& old : defs_) if (old.id == d.id) return Error::duplicate_id;
    if (!(d.operation == "spawn" || d.operation == "molecule" || d.operation == "deposit" ||
          d.operation == "differentiate" || d.operation == "connect" || d.operation == "teach" ||
          d.operation == "supply" || d.operation == "move")) return Error::invalid_task;
    defs_.push_back(d); return Error::ok;
  }
  Error add_image(const v::Action& action) {
    if (image_frozen_) return Error::invalid_state;
    catalog_frozen_ = true;
    return base_.add_image(action) == v::Result::ok ? Error::ok : Error::execution_failed;
  }
  Error finalize_image() noexcept { catalog_frozen_ = true; image_frozen_ = true; return Error::ok; }

  // Atomic transaction: if execution or capacity check fails, the live state
  // remains unchanged. This is an in-memory checkpoint, not physical rollback.
  Error start(std::uint32_t task_id, std::string_view process_id, const v::Action& action) {
    ProcessMachine copy = *this;
    const auto err = copy.start_direct(task_id, process_id, action);
    if (err == Error::ok) *this = std::move(copy);
    return err;
  }
  Error execute(const v::Action& action) {
    ProcessMachine copy = *this;
    const auto err = copy.direct_action(action);
    if (err == Error::ok) *this = std::move(copy);
    return err;
  }
  Error advance(std::int32_t ticks) {
    ProcessMachine copy = *this;
    const auto err = copy.advance_direct(ticks);
    if (err == Error::ok) *this = std::move(copy);
    return err;
  }
  Error clone_image_to(ProcessMachine& output, std::string new_name) const {
    ProcessMachine fresh;
    if (!fresh.name(std::move(new_name))) return Error::invalid_task;
    for (const auto& a : assets_) if (fresh.add_asset(a) != Error::ok) return Error::invalid_task;
    for (const auto& d : defs_) if (fresh.add_definition(d) != Error::ok) return Error::invalid_task;
    for (const auto& a : base_.image()) if (fresh.add_image(a) != Error::ok) return Error::execution_failed;
    fresh.finalize_image();
    output = std::move(fresh);
    return Error::ok;
  }
  // Upgrade a legacy CEVLM/1 scenario with a V2 catalog. The old VLM image
  // and journal are replayed, not copied as trusted memory. Model 2 uses
  // per-tick advances, so legacy SIM1 event hashes are NOT byte-equivalent.
  // We preserve the synthetic observed cell/field result and name it a new model.
  static Error import_v1(const v::VirtualMachine& original,
                         const ProcessMachine& catalog, ProcessMachine& out,
                         std::string name) {
    ProcessMachine next;
    if (!next.name(std::move(name))) return Error::invalid_task;
    for (const auto& a : catalog.assets_)
      if (next.add_asset(a) != Error::ok) return Error::invalid_task;
    for (const auto& d : catalog.defs_)
      if (next.add_definition(d) != Error::ok) return Error::invalid_task;
    for (const auto& a : original.image())
      if (next.add_image(a) != Error::ok) return Error::execution_failed;
    next.finalize_image();
    for (const auto& a : original.journal()) {
      if (!a.is_deposit && a.instruction.op == Op::advance) {
        auto n = a.instruction.value;
        while (n > 0) {
          const auto step = std::min(n, std::int32_t{512});
          if (next.advance(step) != Error::ok) return Error::execution_failed;
          n -= step;
        }
      } else if (next.execute(a) != Error::ok) return Error::execution_failed;
    }
    out = std::move(next);
    return Error::ok;
  }
  std::uint64_t state_hash() const noexcept {
    std::uint64_t h = kFnvOffset;
    detail::hash_u64(h, 2);
    hash_text(h, base_.name());
    detail::hash_u64(h, base_.state_hash());
    for (const auto& a : assets_) { hash_text(h, a.id); hash_text(h, a.role); hash_text(h, a.path); }
    for (const auto& d : defs_) {
      hash_text(h, d.id); hash_text(h, d.operation); hash_text(h, d.asset_id);
      detail::hash_u64(h, d.duration); detail::hash_u64(h, d.weight);
      detail::hash_u64(h, static_cast<std::uint32_t>(d.dx));
      detail::hash_u64(h, static_cast<std::uint32_t>(d.dy));
    }
    for (const auto& t : tasks_) {
      detail::hash_u64(h, t.id); detail::hash_u64(h, t.definition);
      hash_text(h, v::serialize_action(t.action));
      detail::hash_u64(h, t.elapsed); detail::hash_u64(h, t.completed ? 1u : 0u);
      detail::hash_u64(h, static_cast<std::uint32_t>(t.x));
      detail::hash_u64(h, static_cast<std::uint32_t>(t.y));
    }
    for (const auto& j : journal_) {
      hash_text(h, j.start ? j.process_id : j.direct ? "do" : "advance");
      detail::hash_u64(h, j.task_id);
      if (j.start || j.direct) hash_text(h, v::serialize_action(j.action));
      else detail::hash_u64(h, static_cast<std::uint32_t>(j.advance));
    }
    return h;
  }
  std::string serialize(bool sealed = true) const {
    std::ostringstream out;
    out << "CEVLM/2\nname " << base_.name() << "\nmodel 1\nassets\n";
    for (const auto& a : assets_)
      out << "asset " << a.id << ' ' << a.role << ' ' << a.path << '\n';
    out << "endassets\nprocesses\n";
    for (const auto& d : defs_)
      out << "process " << d.id << ' ' << d.operation << ' ' << d.duration << ' '
          << d.weight << ' ' << d.dx << ' ' << d.dy << ' ' << d.asset_id << '\n';
    out << "endprocesses\nimage\n";
    for (const auto& a : base_.image()) out << v::serialize_action(a) << '\n';
    out << "endimage\njournal\n";
    for (const auto& j : journal_) {
      if (j.start) out << "start " << j.task_id << ' ' << j.process_id << ' '
                       << v::serialize_action(j.action) << '\n';
      else if (j.direct) out << "do " << v::serialize_action(j.action) << '\n';
      else out << "advance " << j.advance << '\n';
    }
    out << "endjournal\n";
    if (sealed) out << "seal " << v::hex64(state_hash()) << ' '
                    << v::hex64(base_.state_hash()) << ' ' << tick() << '\n';
    return out.str();
  }
  static LoadResult deserialize(std::string_view bytes, ProcessMachine& destination,
                                bool require_seal = false) {
    if (bytes.size() > 1024 * 1024) return {Error::capacity, 0};
    ProcessMachine staged;
    std::istringstream input{std::string(bytes)};
    std::string line;
    enum class Phase { magic, name, model, assets, processes, image, journal, after, done };
    Phase phase = Phase::magic;
    std::size_t line_no = 0;
    bool saw_seal = false;
    bool saw_assets = false, saw_processes = false, saw_image = false, saw_journal = false;
    while (std::getline(input, line)) {
      ++line_no;
      if (line.size() > 512) return {Error::malformed, line_no};
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty() || line.front() == '#') continue;
      std::vector<std::string> t;
      if (!split(line, t)) return {Error::malformed, line_no};
      if (phase == Phase::magic) {
        if (line != "CEVLM/2") return {Error::invalid_state, line_no};
        phase = Phase::name;
      } else if (phase == Phase::name) {
        if (t.size() != 2 || t[0] != "name" || !staged.name(t[1]))
          return {Error::malformed, line_no};
        phase = Phase::model;
      } else if (phase == Phase::model) {
        if (t.size() != 2 || t[0] != "model" || t[1] != "1")
          return {Error::malformed, line_no};
        phase = Phase::assets;
      } else if (phase == Phase::assets) {
        if (line == "assets" && !saw_assets) { saw_assets = true; continue; }
        if (!saw_assets) return {Error::invalid_state, line_no};
        if (line == "endassets") { phase = Phase::processes; continue; }
        if (t.size() != 4 || t[0] != "asset" ||
            staged.add_asset({t[1], t[2], t[3]}) != Error::ok)
          return {Error::malformed, line_no};
      } else if (phase == Phase::processes) {
        if (line == "processes" && !saw_processes) { saw_processes = true; continue; }
        if (!saw_processes) return {Error::invalid_state, line_no};
        if (line == "endprocesses") { phase = Phase::image; continue; }
        if (t.size() != 8 || t[0] != "process") return {Error::malformed, line_no};
        Definition d{};
        d.id = t[1]; d.operation = t[2]; d.asset_id = t[7];
        if (!v::parse_unsigned(t[3], d.duration) || !v::parse_unsigned(t[4], d.weight) ||
            !v::parse_signed(t[5], d.dx) || !v::parse_signed(t[6], d.dy) ||
            staged.add_definition(d) != Error::ok) return {Error::malformed, line_no};
      } else if (phase == Phase::image) {
        if (line == "image" && !saw_image) { saw_image = true; continue; }
        if (!saw_image) return {Error::invalid_state, line_no};
        if (line == "endimage") { staged.finalize_image(); phase = Phase::journal; continue; }
        v::Action a;
        if (!v::parse_action(line, a) || staged.add_image(a) != Error::ok)
          return {Error::malformed, line_no};
      } else if (phase == Phase::journal) {
        if (line == "journal" && !saw_journal) { saw_journal = true; continue; }
        if (!saw_journal) return {Error::invalid_state, line_no};
        if (line == "endjournal") { phase = Phase::after; continue; }
        if (t[0] == "start") {
          if (t.size() < 5) return {Error::malformed, line_no};
          std::uint32_t id = 0;
          if (!v::parse_unsigned(t[1], id)) return {Error::malformed, line_no};
          std::string action_line;
          for (std::size_t i = 3; i < t.size(); ++i) {
            if (i != 3) action_line += ' ';
            action_line += t[i];
          }
          v::Action action;
          if (!v::parse_action(action_line, action) ||
              staged.start(id, t[2], action) != Error::ok)
            return {Error::invalid_task, line_no};
        } else if (t.size() > 1 && t[0] == "do") {
          std::string action_line;
          for (std::size_t i = 1; i < t.size(); ++i) {
            if (i != 1) action_line += ' ';
            action_line += t[i];
          }
          v::Action action;
          if (!v::parse_action(action_line, action) ||
              staged.execute(action) != Error::ok) return {Error::invalid_task, line_no};
        } else if (t.size() == 2 && t[0] == "advance") {
          std::int32_t ticks = 0;
          if (!v::parse_signed(t[1], ticks) || staged.advance(ticks) != Error::ok)
            return {Error::invalid_task, line_no};
        } else return {Error::malformed, line_no};
      } else if (phase == Phase::after) {
        if (t.size() != 4 || t[0] != "seal") return {Error::malformed, line_no};
        std::uint64_t state = 0, base = 0, tick = 0;
        if (!v::parse_hex64(t[1], state) || !v::parse_hex64(t[2], base))
          return {Error::malformed, line_no};
        const auto [end, ec] = std::from_chars(t[3].data(), t[3].data()+t[3].size(), tick);
        if (ec != std::errc{} || end != t[3].data()+t[3].size() ||
            state != staged.state_hash() || base != staged.base_.state_hash() ||
            tick != staged.tick()) return {Error::checksum, line_no};
        saw_seal = true; phase = Phase::done;
      } else return {Error::invalid_state, line_no};
    }
    if (phase != Phase::after && phase != Phase::done) return {Error::invalid_state, line_no};
    if (require_seal && !saw_seal) return {Error::invalid_state, line_no};
    destination = std::move(staged);
    return {};
  }
};

inline LoadResult read_file(const std::filesystem::path& path, ProcessMachine& out,
                            bool seal_required = false) {
  std::error_code ec;
  auto bytes = std::filesystem::file_size(path, ec);
  if (ec) return {Error::io_error, 0};
  if (bytes > 1024 * 1024) return {Error::capacity, 0};
  std::ifstream f(path, std::ios::binary);
  if (!f) return {Error::io_error, 0};
  std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  if (f.bad()) return {Error::io_error, 0};
  return ProcessMachine::deserialize(content, out, seal_required);
}
inline Error write_file(const std::filesystem::path& path, const ProcessMachine& machine) {
  auto tmp = path.string() + ".tmp";
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) return Error::io_error;
    f << machine.serialize(true);
    f.flush();
    if (!f) return Error::io_error;
  }
  std::error_code ec;
  std::filesystem::rename(tmp, path, ec);
  return ec ? Error::io_error : Error::ok;
}
} // namespace christeye::living::process
