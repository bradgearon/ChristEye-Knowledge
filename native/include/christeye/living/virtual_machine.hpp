#pragma once
// Anaiero VLM1: portable, reproducible DIGITAL-only virtual living machine.
// No chemical formulations, neural stimulation, care automation or biological control.
// Header is host-only; the SIM1 kernel remains a freestanding-capable component.
#include "christeye/living/simulator.hpp"
#include <array>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <limits>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace christeye::living::vlm {

inline constexpr std::size_t kFieldSide = 8;
inline constexpr std::size_t kFieldTypes = 6;
inline constexpr std::int32_t kMaxFieldValue = 100000;
inline constexpr std::size_t kMaxActions = 2000;
using Core = Simulator<64, 32, 128, 2048>;

enum class FieldKind : std::uint8_t { support = 0, guidance, rna, protein, soil, waste };

// All field levels are dimensionless, not measured molecular concentration.
struct FieldGrid {
  std::array<std::int32_t, kFieldTypes * kFieldSide * kFieldSide> values{};
  constexpr std::size_t index(FieldKind f, std::uint32_t x, std::uint32_t y) const noexcept {
    return static_cast<std::size_t>(f) * kFieldSide * kFieldSide +
           static_cast<std::size_t>(y) * kFieldSide + x;
  }
  bool deposit(FieldKind f, std::uint32_t x, std::uint32_t y, std::int32_t amount) noexcept {
    if (static_cast<unsigned>(f) >= kFieldTypes || x >= kFieldSide || y >= kFieldSide || amount < 0)
      return false;
    auto& v = values[index(f, x, y)];
    if (amount > kMaxFieldValue - v) return false;
    v += amount;
    return true;
  }
  std::int32_t at(FieldKind f, std::uint32_t x, std::uint32_t y) const noexcept {
    return (static_cast<unsigned>(f) < kFieldTypes && x < kFieldSide && y < kFieldSide)
        ? values[index(f, x, y)] : 0;
  }
  // Integer-only conservative 4-neighbor transport: q/16 flows to each neighbor.
  // Deliberately simplistic; does NOT simulate biochemical diffusion rates.
  void step() noexcept {
    std::array<std::int64_t, kFieldTypes * kFieldSide * kFieldSide> next{};
    for (unsigned f = 0; f < kFieldTypes; ++f) {
      for (std::uint32_t y = 0; y < kFieldSide; ++y) {
        for (std::uint32_t x = 0; x < kFieldSide; ++x) {
          const auto kind = static_cast<FieldKind>(f);
          const auto idx = index(kind, x, y);
          const std::int32_t flux = values[idx] / 16;
          next[idx] += values[idx];
          const auto transfer = [&](std::uint32_t nx, std::uint32_t ny) noexcept {
            next[idx] -= flux;
            next[index(kind, nx, ny)] += flux;
          };
          if (x > 0) transfer(x - 1, y);
          if (x + 1 < kFieldSide) transfer(x + 1, y);
          if (y > 0) transfer(x, y - 1);
          if (y + 1 < kFieldSide) transfer(x, y + 1);
        }
      }
    }
    for (std::size_t i = 0; i < values.size(); ++i)
      values[i] = static_cast<std::int32_t>(next[i]);
  }
  std::int64_t total(FieldKind f) const noexcept {
    std::int64_t sum = 0;
    if (static_cast<unsigned>(f) >= kFieldTypes) return sum;
    for (std::uint32_t y = 0; y < kFieldSide; ++y)
      for (std::uint32_t x = 0; x < kFieldSide; ++x)
        sum += at(f, x, y);
    return sum;
  }
  std::uint64_t hash() const noexcept {
    std::uint64_t h = kFnvOffset;
    for (auto v : values) detail::hash_u64(h, static_cast<std::uint32_t>(v));
    return h;
  }
};

struct Action {
  bool is_deposit = false;
  Instruction instruction{};
  FieldKind field = FieldKind::support;
  std::uint32_t x = 0, y = 0;
  std::int32_t amount = 0;
};

enum class Result : std::uint8_t {
  ok, invalid_setup, malformed_line, model_incompatible, invalid_action,
  kernel_rejected, full, checksum_mismatch, io_error
};
struct LoadResult { Result result = Result::ok; std::size_t line = 0; };

inline bool safe_name(std::string_view n) noexcept {
  if (n.empty() || n.size() > 64) return false;
  for (char c : n)
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.')) return false;
  return true;
}
inline bool parse_unsigned(std::string_view text, std::uint32_t& v) noexcept {
  if (text.empty()) return false;
  auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), v);
  return ec == std::errc{} && end == text.data() + text.size();
}
inline bool parse_signed(std::string_view text, std::int32_t& v) noexcept {
  if (text.empty()) return false;
  auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), v);
  return ec == std::errc{} && end == text.data() + text.size();
}
inline bool parse_hex64(std::string_view text, std::uint64_t& v) noexcept {
  if (text.size() != 16) return false;
  auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), v, 16);
  return ec == std::errc{} && end == text.data() + text.size();
}
inline std::string hex64(std::uint64_t v) {
  std::ostringstream os;
  os << std::hex << std::setfill('0') << std::setw(16) << v;
  return os.str();
}
inline const char* field_name(FieldKind f) noexcept {
  switch (f) {
    case FieldKind::support: return "support";
    case FieldKind::guidance: return "guidance";
    case FieldKind::rna: return "rna";
    case FieldKind::protein: return "protein";
    case FieldKind::soil: return "soil";
    case FieldKind::waste: return "waste";
  }
  return "invalid";
}
inline bool parse_field(std::string_view n, FieldKind& f) noexcept {
  for (unsigned i = 0; i < kFieldTypes; ++i) {
    auto v = static_cast<FieldKind>(i);
    if (n == field_name(v)) { f = v; return true; }
  }
  return false;
}
inline const char* cell_name(CellKind f) noexcept {
  switch (f) {
    case CellKind::crest_progenitor: return "crest_progenitor";
    case CellKind::blood_carrier: return "blood_carrier";
    case CellKind::neural_progenitor: return "neural_progenitor";
    case CellKind::peripheral_neuron: return "peripheral_neuron";
    case CellKind::central_neuron: return "central_neuron";
    case CellKind::astrocyte: return "astrocyte";
    case CellKind::schwann: return "schwann";
    case CellKind::microglia: return "microglia";
    case CellKind::endothelial: return "endothelial";
  }
  return "invalid";
}
inline bool parse_cell(std::string_view n, std::uint32_t& v) noexcept {
  for (unsigned i = 1; i <= static_cast<unsigned>(CellKind::endothelial); ++i) {
    if (n == cell_name(static_cast<CellKind>(i))) { v = i; return true; }
  }
  return false;
}
inline const char* molecule_name(MoleculeKind f) noexcept {
  switch (f) {
    case MoleculeKind::support: return "support";
    case MoleculeKind::guidance_proxy: return "guidance_proxy";
    case MoleculeKind::transport_proxy: return "transport_proxy";
    case MoleculeKind::activity_proxy: return "activity_proxy";
    case MoleculeKind::rna_proxy: return "rna_proxy";
    case MoleculeKind::protein_proxy: return "protein_proxy";
    case MoleculeKind::waste_proxy: return "waste_proxy";
  }
  return "invalid";
}
inline bool parse_molecule(std::string_view n, std::uint32_t& v) noexcept {
  for (unsigned i = 1; i <= static_cast<unsigned>(MoleculeKind::waste_proxy); ++i) {
    if (n == molecule_name(static_cast<MoleculeKind>(i))) { v = i; return true; }
  }
  return false;
}
inline bool tokenize(std::string_view text, std::vector<std::string>& tokens) {
  std::istringstream in{std::string(text)};
  std::string token;
  while (in >> token) {
    if (tokens.size() >= 8 || token.size() > 64) return false;
    tokens.push_back(token);
  }
  return true;
}
// All required numbers must be supplied. Unknown tokens and trailing data fail closed.
inline bool parse_action(std::string_view text, Action& out) {
  std::vector<std::string> t;
  if (!tokenize(text, t) || t.empty()) return false;
  auto size = t.size();
  const auto u = [&](std::size_t i, std::uint32_t& val) {
    return i < size && parse_unsigned(t[i], val);
  };
  const auto s = [&](std::size_t i, std::int32_t& val) {
    return i < size && parse_signed(t[i], val);
  };
  Action a{};
  if (t[0] == "deposit" && size == 5) {
    a.is_deposit = true;
    if (!parse_field(t[1], a.field) || !u(2, a.x) || !u(3, a.y) || !s(4, a.amount)) return false;
  } else if (t[0] == "spawn" && size == 6) {
    a.instruction.op = Op::spawn;
    if (!u(1, a.instruction.subject) || !parse_cell(t[2], a.instruction.target) ||
        !s(3, a.instruction.value) || !s(4, a.instruction.extra) ||
        !u(5, a.instruction.parent)) return false;
  } else if (t[0] == "molecule" && size == 5) {
    a.instruction.op = Op::define_molecule;
    if (!u(1, a.instruction.subject) || !parse_molecule(t[2], a.instruction.target) ||
        !s(3, a.instruction.value) || !s(4, a.instruction.extra)) return false;
  } else if (t[0] == "supply" && size == 3) {
    a.instruction.op = Op::supply;
    if (!u(1, a.instruction.subject) || !s(2, a.instruction.value)) return false;
  } else if (t[0] == "move" && size == 4) {
    a.instruction.op = Op::move;
    if (!u(1, a.instruction.subject) || !s(2, a.instruction.value) ||
        !s(3, a.instruction.extra)) return false;
  } else if (t[0] == "differentiate" && size == 3) {
    a.instruction.op = Op::differentiate;
    if (!u(1, a.instruction.subject) || !parse_cell(t[2], a.instruction.target)) return false;
  } else if (t[0] == "connect" && size == 4) {
    a.instruction.op = Op::connect;
    if (!u(1, a.instruction.subject) || !u(2, a.instruction.target) ||
        !s(3, a.instruction.value)) return false;
  } else if (t[0] == "teach" && size == 4) {
    a.instruction.op = Op::teach_symbol;
    if (!u(1, a.instruction.subject) || !u(2, a.instruction.target) ||
        !s(3, a.instruction.value)) return false;
  } else if (t[0] == "advance" && size == 2) {
    a.instruction.op = Op::advance;
    if (!s(1, a.instruction.value)) return false;
  } else if (t[0] == "observe" && size == 1) {
    a.instruction.op = Op::observe;
  } else return false;
  out = a;
  return true;
}
inline std::string serialize_action(const Action& a) {
  std::ostringstream os;
  if (a.is_deposit) {
    os << "deposit " << field_name(a.field) << ' ' << a.x << ' ' << a.y << ' ' << a.amount;
    return os.str();
  }
  const auto& i = a.instruction;
  switch (i.op) {
    case Op::spawn: os << "spawn " << i.subject << ' ' << cell_name(static_cast<CellKind>(i.target)) << ' ' << i.value << ' ' << i.extra << ' ' << i.parent; break;
    case Op::define_molecule: os << "molecule " << i.subject << ' ' << molecule_name(static_cast<MoleculeKind>(i.target)) << ' ' << i.value << ' ' << i.extra; break;
    case Op::supply: os << "supply " << i.subject << ' ' << i.value; break;
    case Op::move: os << "move " << i.subject << ' ' << i.value << ' ' << i.extra; break;
    case Op::differentiate: os << "differentiate " << i.subject << ' ' << cell_name(static_cast<CellKind>(i.target)); break;
    case Op::connect: os << "connect " << i.subject << ' ' << i.target << ' ' << i.value; break;
    case Op::teach_symbol: os << "teach " << i.subject << ' ' << i.target << ' ' << i.value; break;
    case Op::advance: os << "advance " << i.value; break;
    case Op::observe: os << "observe"; break;
  }
  return os.str();
}

class VirtualMachine {
  std::string name_ = "new-colony";
  Core core_{};
  FieldGrid soil_{};
  std::vector<Action> image_{};
  std::vector<Action> journal_{};
  bool frozen_image_ = false;
  std::uint64_t action_chain_ = kFnvOffset; // VLM image + journal; synthetic only

  Result apply_internal(const Action& action, bool image) {
    if (image && frozen_image_) return Result::invalid_setup;
    if (image_.size() + journal_.size() >= kMaxActions) return Result::full;
    if (action.is_deposit) {
      if (!soil_.deposit(action.field, action.x, action.y, action.amount)) return Result::invalid_action;
    } else {
      const auto status = core_.apply(action.instruction);
      if (status != Status::ok) return Result::kernel_rejected;
      if (action.instruction.op == Op::advance)
        for (std::int32_t n = 0; n < action.instruction.value; ++n) soil_.step();
    }
    // Preserve causal accounting for VLM-only field deposits as well as SIM1
    // instructions, including image/runtime boundary. Non-cryptographic FNV.
    detail::hash_u64(action_chain_, image ? 1u : 2u);
    detail::hash_u64(action_chain_, action.is_deposit ? 1u : 0u);
    if (action.is_deposit) {
      detail::hash_u64(action_chain_, static_cast<unsigned>(action.field));
      detail::hash_u64(action_chain_, action.x);
      detail::hash_u64(action_chain_, action.y);
      detail::hash_u64(action_chain_, static_cast<std::uint32_t>(action.amount));
    } else {
      const auto& i = action.instruction;
      detail::hash_u64(action_chain_, static_cast<unsigned>(i.op));
      detail::hash_u64(action_chain_, i.subject);
      detail::hash_u64(action_chain_, i.target);
      detail::hash_u64(action_chain_, static_cast<std::uint32_t>(i.value));
      detail::hash_u64(action_chain_, static_cast<std::uint32_t>(i.extra));
      detail::hash_u64(action_chain_, i.parent);
    }
    (image ? image_ : journal_).push_back(action);
    if (!image) frozen_image_ = true;
    return Result::ok;
  }
public:
  VirtualMachine() = default;
  bool set_name(std::string value) {
    if (!safe_name(value)) return false;
    name_ = std::move(value);
    return true;
  }
  const std::string& name() const noexcept { return name_; }
  const Core& core() const noexcept { return core_; }
  const FieldGrid& soil() const noexcept { return soil_; }
  std::uint64_t action_chain() const noexcept { return action_chain_; }
  const std::vector<Action>& image() const noexcept { return image_; }
  const std::vector<Action>& journal() const noexcept { return journal_; }
  Result add_image(const Action& action) { return apply_internal(action, true); }
  Result apply(const Action& action) { return apply_internal(action, false); }
  Result advance(std::int32_t ticks) {
    return apply(Action{false, Instruction{Op::advance, 0, 0, ticks}});
  }
  // Clone the original image, leaving all runtime changes behind. Check return.
  Result clone_image_to(VirtualMachine& destination, std::string name) const {
    if (!safe_name(name)) return Result::invalid_setup;
    VirtualMachine staging;
    if (!staging.set_name(std::move(name))) return Result::invalid_setup;
    for (auto action : image_) {
      if (staging.add_image(action) != Result::ok) return Result::kernel_rejected;
    }
    destination = std::move(staging);
    return Result::ok;
  }
  std::uint64_t state_hash() const noexcept {
    std::uint64_t hash = kFnvOffset;
    detail::hash_u64(hash, kModelRevision);
    detail::hash_u64(hash, core_.state_hash());
    detail::hash_u64(hash, core_.event_chain());
    detail::hash_u64(hash, soil_.hash());
    detail::hash_u64(hash, action_chain_);
    detail::hash_u64(hash, image_.size());
    detail::hash_u64(hash, journal_.size());
    return hash;
  }
  std::string serialize(bool seal = true) const {
    std::ostringstream o;
    o << "CEVLM/1\nname " << name_ << "\nmodel " << kModelRevision << "\nimage\n";
    for (const auto& a : image_) o << serialize_action(a) << '\n';
    o << "endimage\njournal\n";
    for (const auto& a : journal_) o << serialize_action(a) << '\n';
    o << "endjournal\n";
    if (seal) o << "seal " << hex64(core_.state_hash()) << ' ' << hex64(core_.event_chain())
                << ' ' << hex64(soil_.hash()) << ' ' << hex64(state_hash())
                << ' ' << core_.tick() << '\n';
    return o.str();
  }
  static LoadResult deserialize(std::string_view body, VirtualMachine& out, bool require_seal = false) {
    if (body.size() > 1024 * 1024) return {Result::full, 0};
    VirtualMachine staging;
    std::istringstream input{std::string(body)};
    std::string line;
    std::size_t line_no = 0;
    enum class Phase { magic, name, model, image_start, image, journal_start,
                       journal, after_journal, done };
    Phase phase = Phase::magic;
    bool saw_seal = false;
    while (std::getline(input, line)) {
      ++line_no;
      if (line.size() > 256) return {Result::malformed_line, line_no};
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.empty() || line[0] == '#') continue;
      std::vector<std::string> t;
      if (!tokenize(line, t) || t.empty()) return {Result::malformed_line, line_no};
      switch (phase) {
        case Phase::magic:
          if (line != "CEVLM/1") return {Result::invalid_setup, line_no};
          phase = Phase::name;
          break;
        case Phase::name:
          if (t.size() != 2 || t[0] != "name" || !staging.set_name(t[1]))
            return {Result::invalid_setup, line_no};
          phase = Phase::model;
          break;
        case Phase::model:
          if (t.size() != 2 || t[0] != "model" || t[1] != "1")
            return {Result::model_incompatible, line_no};
          phase = Phase::image_start;
          break;
        case Phase::image_start:
          if (line != "image") return {Result::invalid_setup, line_no};
          phase = Phase::image;
          break;
        case Phase::image:
          if (line == "endimage") { phase = Phase::journal_start; break; }
          { Action a;
            if (!parse_action(line, a)) return {Result::malformed_line, line_no};
            auto r = staging.add_image(a);
            if (r != Result::ok) return {r, line_no}; }
          break;
        case Phase::journal_start:
          if (line != "journal") return {Result::invalid_setup, line_no};
          phase = Phase::journal;
          break;
        case Phase::journal:
          if (line == "endjournal") { phase = Phase::after_journal; break; }
          { Action a;
            if (!parse_action(line, a)) return {Result::malformed_line, line_no};
            auto r = staging.apply(a);
            if (r != Result::ok) return {r, line_no}; }
          break;
        case Phase::after_journal: {
          if (t.size() != 6 || t[0] != "seal") return {Result::invalid_setup, line_no};
          std::uint64_t sh = 0, ch = 0, fh = 0, vh = 0, tick = 0;
          if (!parse_hex64(t[1], sh) || !parse_hex64(t[2], ch) ||
              !parse_hex64(t[3], fh) || !parse_hex64(t[4], vh))
            return {Result::malformed_line, line_no};
          const auto [end_tick, ec] = std::from_chars(t[5].data(),
                                                    t[5].data() + t[5].size(), tick);
          if (ec != std::errc{} || end_tick != t[5].data() + t[5].size())
            return {Result::malformed_line, line_no};
          if (sh != staging.core_.state_hash() || ch != staging.core_.event_chain() ||
              fh != staging.soil_.hash() || vh != staging.state_hash() ||
              tick != staging.core_.tick())
            return {Result::checksum_mismatch, line_no};
          saw_seal = true;
          phase = Phase::done;
          break;
        }
        case Phase::done:
          return {Result::invalid_setup, line_no};
      }
    }
    if (phase != Phase::after_journal && phase != Phase::done)
      return {Result::invalid_setup, line_no};
    if (require_seal && !saw_seal) return {Result::invalid_setup, line_no};
    out = std::move(staging);
    return {};
  }

};

inline LoadResult read_file(const std::filesystem::path& file, VirtualMachine& out, bool require_seal = false) {
  std::error_code ec;
  const auto bytes = std::filesystem::file_size(file, ec);
  if (ec) return {Result::io_error, 0};
  if (bytes > 1024 * 1024) return {Result::full, 0};
  std::ifstream in(file, std::ios::binary);
  if (!in) return {Result::io_error, 0};
  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  if (!in.eof() && in.bad()) return {Result::io_error, 0};
  return VirtualMachine::deserialize(data, out, require_seal);
}
inline Result write_file(const std::filesystem::path& file, const VirtualMachine& vm) {
  const auto tmp = file.string() + ".tmp";
  {
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return Result::io_error;
    out << vm.serialize(true);
    out.flush();
    if (!out) return Result::io_error;
  }
  std::error_code ec;
  std::filesystem::rename(tmp, file, ec);
  if (ec) return Result::io_error;
  return Result::ok;
}
} // namespace christeye::living::vlm
