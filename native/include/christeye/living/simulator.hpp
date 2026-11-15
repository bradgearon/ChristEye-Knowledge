#pragma once
// ChristEye Anaiero SIM1: deterministic, bounded DIGITAL research surrogate.
// Symbolic quantities are dimensionless and are NOT biochemical rate laws,
// cell protocols, predictions, or evidence of biological learning.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace christeye::living {

enum class CellKind : std::uint8_t {
  crest_progenitor = 1, blood_carrier, neural_progenitor,
  peripheral_neuron, central_neuron, astrocyte,
  schwann, microglia, endothelial
};
enum class MoleculeKind : std::uint8_t {
  support = 1, guidance_proxy, transport_proxy, activity_proxy,
  rna_proxy, protein_proxy, waste_proxy
};
enum class Op : std::uint8_t {
  spawn = 1, define_molecule, supply, move, differentiate,
  connect, teach_symbol, advance, observe
};
enum class Status : std::uint8_t {
  ok, invalid_operation, invalid_id, invalid_kind, invalid_amount,
  illegal_lineage, incompatible_connection, capacity_exceeded,
  out_of_bounds, duplicate_id, missing_definition, overflow
};

struct Instruction {
  Op op = Op::observe;
  std::uint32_t subject = 0; // cell or molecule ID, depending on opcode
  std::uint32_t target = 0;  // cell/molecule kind, peer ID, or symbol ID
  std::int32_t value = 0;    // x, delta x, quantity, connection weight, or ticks
  std::int32_t extra = 0;    // y, delta y, or optional label
  std::uint32_t parent = 0;  // existing cell ID or 0 for external initial seed
};
struct Cell {
  std::uint32_t id = 0;
  std::uint32_t parent_id = 0;
  CellKind kind = CellKind::crest_progenitor;
  std::int32_t x = 0, y = 0;             // arbitrary synthetic spatial units
  std::int32_t energy = 400;            // bounded [0,1000], dimensionless
  std::int32_t maturity = 0;
  std::int32_t neurite_proxy = 0;
  std::int32_t rna_proxy = 0;
  std::int32_t learning_proxy = 0;
  std::uint32_t last_symbol = 0;
  bool viable = true;
};
struct Molecule {
  std::uint32_t id = 0;
  MoleculeKind kind = MoleculeKind::support;
  std::int32_t quantity = 0;  // dimensionless symbolic inventory, [0, 100000]
  std::int32_t loss_per_tick = 0;
};
struct Connection {
  std::uint32_t source = 0, target = 0;
  std::uint16_t weight = 0; // a synthetic graph edge, not a measured synapse
};
inline constexpr std::uint32_t kModelRevision = 1;
struct Evidence {
  std::uint64_t sequence = 0;
  std::uint64_t tick = 0;
  Op operation = Op::observe;
  std::uint32_t subject = 0, target = 0;
  Status result = Status::ok;
  std::uint64_t state_hash = 0; // FNV-1a consistency check: NOT cryptographic
  std::uint32_t model_revision = kModelRevision;
  std::uint64_t parent_hash = 0;
};
struct Metrics {
  std::uint64_t tick = 0;
  std::uint32_t cells = 0, viable_cells = 0, links = 0, molecules = 0;
  std::int64_t total_energy = 0;
  std::int64_t neurite_proxy = 0, rna_proxy = 0, learning_proxy = 0;
};

// Explicit deterministic simulator law table, NOT calibrated physiology.
inline constexpr std::int32_t kMaxCellState = 1000;
inline constexpr std::int32_t kMaxMoleculeQuantity = 100000;
inline constexpr std::int32_t kWorldMax = 4096;
inline constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
inline constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

namespace detail {
constexpr bool valid_cell_kind(std::uint32_t raw) noexcept {
  return raw >= 1 && raw <= static_cast<std::uint32_t>(CellKind::endothelial);
}
constexpr bool valid_molecule_kind(std::uint32_t raw) noexcept {
  return raw >= 1 && raw <= static_cast<std::uint32_t>(MoleculeKind::waste_proxy);
}
constexpr std::int32_t bounded_add(std::int32_t a, std::int32_t delta,
                                   std::int32_t limit) noexcept {
  const std::int64_t value = static_cast<std::int64_t>(a) + delta;
  return static_cast<std::int32_t>(value < 0 ? 0 : value > limit ? limit : value);
}
constexpr void hash_u64(std::uint64_t& h, std::uint64_t value) noexcept {
  for (unsigned i = 0; i < 8; ++i) {
    h ^= static_cast<std::uint8_t>(value & 0xffu);
    h *= kFnvPrime;
    value >>= 8;
  }
}
constexpr bool lineage_allows(CellKind from, CellKind to) noexcept {
  if (from == CellKind::crest_progenitor)
    return to == CellKind::peripheral_neuron || to == CellKind::schwann;
  if (from == CellKind::neural_progenitor)
    return to == CellKind::central_neuron || to == CellKind::astrocyte;
  return false;
}
constexpr bool neuron(CellKind kind) noexcept {
  return kind == CellKind::peripheral_neuron || kind == CellKind::central_neuron;
}
} // namespace detail

// Fully caller-owned value type; copying it creates an exact checkpoint.
// No dynamically allocated memory, background timer, actor thread or
// laboratory driver. One owner executes instructions serially.
template<std::size_t MaxCells = 64, std::size_t MaxMolecules = 32,
         std::size_t MaxConnections = 128, std::size_t MaxEvents = 2048>
class Simulator {
  static_assert(MaxCells && MaxMolecules && MaxConnections && MaxEvents);
  std::array<Cell, MaxCells> cells_{};
  std::array<Molecule, MaxMolecules> molecules_{};
  std::array<Connection, MaxConnections> edges_{};
  std::array<Evidence, MaxEvents> events_{};
  std::size_t cell_count_ = 0, molecule_count_ = 0, edge_count_ = 0, event_count_ = 0;
  std::uint64_t tick_ = 0;
  std::uint64_t event_chain_ = kFnvOffset;
  using OnEvidence = void (*)(void*, const Evidence&) noexcept;
  OnEvidence callback_ = nullptr;
  void* context_ = nullptr;

  Cell* find_cell(std::uint32_t id) noexcept {
    for (std::size_t i = 0; i < cell_count_; ++i)
      if (cells_[i].id == id) return &cells_[i];
    return nullptr;
  }
  const Cell* find_cell(std::uint32_t id) const noexcept {
    for (std::size_t i = 0; i < cell_count_; ++i)
      if (cells_[i].id == id) return &cells_[i];
    return nullptr;
  }
  Molecule* find_molecule(std::uint32_t id) noexcept {
    for (std::size_t i = 0; i < molecule_count_; ++i)
      if (molecules_[i].id == id) return &molecules_[i];
    return nullptr;
  }
  const Molecule* find_molecule(std::uint32_t id) const noexcept {
    for (std::size_t i = 0; i < molecule_count_; ++i)
      if (molecules_[i].id == id) return &molecules_[i];
    return nullptr;
  }
  bool linked(std::uint32_t a, std::uint32_t b) const noexcept {
    for (std::size_t i = 0; i < edge_count_; ++i)
      if (edges_[i].source == a && edges_[i].target == b) return true;
    return false;
  }
  std::int32_t pool(MoleculeKind kind) const noexcept {
    std::int32_t result = 0;
    for (std::size_t i = 0; i < molecule_count_; ++i)
      if (molecules_[i].kind == kind)
        result = detail::bounded_add(result, molecules_[i].quantity, kMaxMoleculeQuantity);
    return result;
  }
  void tick_one() noexcept {
    ++tick_;
    for (std::size_t i = 0; i < molecule_count_; ++i) {
      auto& m = molecules_[i];
      m.quantity = detail::bounded_add(m.quantity, -m.loss_per_tick, kMaxMoleculeQuantity);
    }
    const std::int32_t support = pool(MoleculeKind::support);
    const std::int32_t guidance = pool(MoleculeKind::guidance_proxy);
    const std::int32_t rna = pool(MoleculeKind::rna_proxy);
    // Every constant below is an **arbitrary synthetic law** for exercising
    // software coordination. No biological parameter or effect is asserted.
    for (std::size_t i = 0; i < cell_count_; ++i) {
      auto& c = cells_[i];
      if (!c.viable) continue;
      const std::int32_t cost = c.kind == CellKind::blood_carrier ? 1 : 3;
      const std::int32_t gain = support > 0 ? 2 : 0;
      c.energy = detail::bounded_add(c.energy, gain - cost, kMaxCellState);
      if (c.energy == 0) { c.viable = false; continue; }
      c.maturity = detail::bounded_add(c.maturity, support > 0 ? 2 : 1, kMaxCellState);
      if (detail::neuron(c.kind)) {
        c.neurite_proxy = detail::bounded_add(c.neurite_proxy,
                                                guidance > 0 ? 2 : 1, kMaxCellState);
        c.rna_proxy = detail::bounded_add(c.rna_proxy,
                                            rna > 0 ? 2 : 0, kMaxCellState);
      }
      if (c.kind == CellKind::blood_carrier) {
        // A visible moving agent, not a model of haemodynamics.
        c.x = (c.x + 1) % (kWorldMax + 1);
      }
    }
    for (std::size_t i = 0; i < edge_count_; ++i) {
      auto& edge = edges_[i];
      const auto* a = find_cell(edge.source);
      const auto* b = find_cell(edge.target);
      if (a && b && a->viable && b->viable && edge.weight < 1000)
        ++edge.weight; // synthetic edge strengthening, never measured memory
    }
  }

  void record(const Instruction& op) noexcept {
    Evidence event{};
    event.sequence = event_count_ + 1;
    event.tick = tick_;
    event.operation = op.op;
    event.subject = op.subject;
    event.target = op.target;
    event.state_hash = state_hash();
    event.parent_hash = event_chain_;
    std::uint64_t next = event_chain_;
    detail::hash_u64(next, event.sequence);
    detail::hash_u64(next, event.tick);
    detail::hash_u64(next, static_cast<std::uint8_t>(event.operation));
    detail::hash_u64(next, event.subject);
    detail::hash_u64(next, event.target);
    detail::hash_u64(next, event.state_hash);
    detail::hash_u64(next, event.model_revision);
    event_chain_ = next;
    events_[event_count_++] = event;
    if (callback_) callback_(context_, event);
  }
public:
  Simulator() noexcept = default;
  // Never allow callback re-entry into apply(). Callback has no ownership
  // of a stored event after state restore; its value is copied on dispatch.
  void set_evidence_callback(OnEvidence callback, void* context) noexcept {
    callback_ = callback;
    context_ = context;
  }
  const Cell* cell(std::uint32_t id) const noexcept { return find_cell(id); }
  const Molecule* molecule(std::uint32_t id) const noexcept { return find_molecule(id); }
  std::size_t event_count() const noexcept { return event_count_; }
  std::size_t connection_count() const noexcept { return edge_count_; }
  const Connection* connection(std::size_t index) const noexcept {
    return index < edge_count_ ? &edges_[index] : nullptr;
  }
  const Evidence* event(std::size_t index) const noexcept {
    return index < event_count_ ? &events_[index] : nullptr;
  }
  std::uint64_t tick() const noexcept { return tick_; }
  std::uint64_t event_chain() const noexcept { return event_chain_; }
  std::uint64_t state_hash() const noexcept {
    std::uint64_t h = kFnvOffset;
    detail::hash_u64(h, kModelRevision);
    detail::hash_u64(h, tick_);
    detail::hash_u64(h, cell_count_);
    detail::hash_u64(h, molecule_count_);
    detail::hash_u64(h, edge_count_);
    for (std::size_t i = 0; i < cell_count_; ++i) {
      const Cell& c = cells_[i];
      detail::hash_u64(h, c.id);
      detail::hash_u64(h, c.parent_id);
      detail::hash_u64(h, static_cast<std::uint8_t>(c.kind));
      detail::hash_u64(h, static_cast<std::uint32_t>(c.x));
      detail::hash_u64(h, static_cast<std::uint32_t>(c.y));
      detail::hash_u64(h, static_cast<std::uint32_t>(c.energy));
      detail::hash_u64(h, static_cast<std::uint32_t>(c.maturity));
      detail::hash_u64(h, static_cast<std::uint32_t>(c.neurite_proxy));
      detail::hash_u64(h, static_cast<std::uint32_t>(c.rna_proxy));
      detail::hash_u64(h, static_cast<std::uint32_t>(c.learning_proxy));
      detail::hash_u64(h, c.last_symbol);
      detail::hash_u64(h, c.viable ? 1 : 0);
    }
    for (std::size_t i = 0; i < molecule_count_; ++i) {
      const Molecule& m = molecules_[i];
      detail::hash_u64(h, m.id);
      detail::hash_u64(h, static_cast<std::uint8_t>(m.kind));
      detail::hash_u64(h, static_cast<std::uint32_t>(m.quantity));
      detail::hash_u64(h, static_cast<std::uint32_t>(m.loss_per_tick));
    }
    for (std::size_t i = 0; i < edge_count_; ++i) {
      const auto& e = edges_[i];
      detail::hash_u64(h, e.source);
      detail::hash_u64(h, e.target);
      detail::hash_u64(h, e.weight);
    }
    return h;
  }
  Metrics metrics() const noexcept {
    Metrics m{};
    m.tick = tick_;
    m.cells = static_cast<std::uint32_t>(cell_count_);
    m.links = static_cast<std::uint32_t>(edge_count_);
    m.molecules = static_cast<std::uint32_t>(molecule_count_);
    for (std::size_t i = 0; i < cell_count_; ++i) {
      const auto& c = cells_[i];
      m.viable_cells += c.viable ? 1u : 0u;
      m.total_energy += c.energy;
      m.neurite_proxy += c.neurite_proxy;
      m.rna_proxy += c.rna_proxy;
      m.learning_proxy += c.learning_proxy;
    }
    return m;
  }
  Status apply(const Instruction& op) noexcept {
    // No mutation on rejection. Journal full is prechecked for all opcodes.
    if (event_count_ >= MaxEvents) return Status::capacity_exceeded;
    switch (op.op) {
      case Op::spawn: {
        if (!op.subject) return Status::invalid_id;
        if (find_cell(op.subject)) return Status::duplicate_id;
        if (!detail::valid_cell_kind(op.target)) return Status::invalid_kind;
        if (op.value < 0 || op.value > kWorldMax ||
            op.extra < 0 || op.extra > kWorldMax) return Status::out_of_bounds;
        if (cell_count_ == MaxCells) return Status::capacity_exceeded;
        if (op.parent != 0) {
          const Cell* parent = find_cell(op.parent);
          if (!parent) return Status::invalid_id;
          const auto target_kind = static_cast<CellKind>(op.target);
          if (parent->kind != target_kind &&
              !detail::lineage_allows(parent->kind, target_kind))
            return Status::illegal_lineage;
        }
        cells_[cell_count_++] = Cell{op.subject, op.parent,
           static_cast<CellKind>(op.target), op.value, op.extra};
        break;
      }
      case Op::define_molecule: {
        if (!op.subject) return Status::invalid_id;
        if (find_molecule(op.subject)) return Status::duplicate_id;
        if (!detail::valid_molecule_kind(op.target)) return Status::invalid_kind;
        if (op.value < 0 || op.value > kMaxMoleculeQuantity ||
            op.extra < 0 || op.extra > kMaxMoleculeQuantity)
          return Status::invalid_amount;
        if (molecule_count_ == MaxMolecules) return Status::capacity_exceeded;
        molecules_[molecule_count_++] = Molecule{op.subject,
          static_cast<MoleculeKind>(op.target), op.value, op.extra};
        break;
      }
      case Op::supply: {
        Molecule* m = find_molecule(op.subject);
        if (!m) return Status::missing_definition;
        if (op.value < -kMaxMoleculeQuantity || op.value > kMaxMoleculeQuantity)
          return Status::invalid_amount;
        m->quantity = detail::bounded_add(m->quantity, op.value, kMaxMoleculeQuantity);
        break;
      }
      case Op::move: {
        Cell* c = find_cell(op.subject);
        if (!c) return Status::invalid_id;
        const std::int64_t x = static_cast<std::int64_t>(c->x) + op.value;
        const std::int64_t y = static_cast<std::int64_t>(c->y) + op.extra;
        if (x < 0 || x > kWorldMax || y < 0 || y > kWorldMax)
          return Status::out_of_bounds;
        c->x = static_cast<std::int32_t>(x);
        c->y = static_cast<std::int32_t>(y);
        break;
      }
      case Op::differentiate: {
        Cell* c = find_cell(op.subject);
        if (!c) return Status::invalid_id;
        if (!detail::valid_cell_kind(op.target)) return Status::invalid_kind;
        auto dst = static_cast<CellKind>(op.target);
        if (!detail::lineage_allows(c->kind, dst)) return Status::illegal_lineage;
        if (!c->viable) return Status::illegal_lineage;
        c->kind = dst;
        break;
      }
      case Op::connect: {
        Cell* a = find_cell(op.subject);
        Cell* b = find_cell(op.target);
        if (!a || !b || a == b) return Status::invalid_id;
        if (!detail::neuron(a->kind) || !detail::neuron(b->kind) ||
            !a->viable || !b->viable) return Status::incompatible_connection;
        if (op.value < 1 || op.value > 1000) return Status::invalid_amount;
        if (linked(op.subject, op.target)) return Status::duplicate_id;
        if (edge_count_ == MaxConnections) return Status::capacity_exceeded;
        edges_[edge_count_++] = Connection{op.subject, op.target,
                                        static_cast<std::uint16_t>(op.value)};
        break;
      }
      case Op::teach_symbol: {
        Cell* c = find_cell(op.subject);
        if (!c) return Status::invalid_id;
        if (!detail::neuron(c->kind) || !c->viable) return Status::invalid_kind;
        if (op.target == 0 || op.value < 0 || op.value > 1000)
          return Status::invalid_amount;
        c->last_symbol = op.target;
        c->learning_proxy = detail::bounded_add(c->learning_proxy,
                                                 op.value, kMaxCellState);
        // Synthetic counter; not learned speech or a physiological engram.
        break;
      }
      case Op::advance: {
        if (op.value <= 0 || op.value > 10000 ||
            tick_ > std::numeric_limits<std::uint64_t>::max() -
                    static_cast<std::uint64_t>(op.value)) return Status::overflow;
        for (std::int32_t t = 0; t < op.value; ++t) tick_one();
        break;
      }
      case Op::observe: break;
      default: return Status::invalid_operation;
    }
    record(op);
    return Status::ok;
  }
};

} // namespace christeye::living
