#include "christeye/living/simulator.hpp"
#include "christeye/living/bytecode.hpp"
#include "christeye/wind/packet.hpp"
#include "christeye/wind/scheduler.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>

using namespace christeye::living;
using namespace christeye::wind;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " #expr << "\n"; std::exit(1); } } while (false)

using Sim = Simulator<16, 8, 16, 128>;
static constexpr std::array<Instruction, 15> kProgram{{
  {Op::spawn, 1, static_cast<std::uint32_t>(CellKind::crest_progenitor), 10, 12},
  {Op::spawn, 2, static_cast<std::uint32_t>(CellKind::blood_carrier), 3, 5},
  {Op::spawn, 3, static_cast<std::uint32_t>(CellKind::neural_progenitor), 12, 11},
  {Op::spawn, 4, static_cast<std::uint32_t>(CellKind::central_neuron), 13, 11},
  {Op::spawn, 5, static_cast<std::uint32_t>(CellKind::neural_progenitor), 16, 12},
  {Op::differentiate, 1, static_cast<std::uint32_t>(CellKind::peripheral_neuron)},
  {Op::differentiate, 3, static_cast<std::uint32_t>(CellKind::central_neuron)},
  {Op::differentiate, 5, static_cast<std::uint32_t>(CellKind::astrocyte)},
  {Op::define_molecule, 100, static_cast<std::uint32_t>(MoleculeKind::support), 70, 1},
  {Op::define_molecule, 101, static_cast<std::uint32_t>(MoleculeKind::guidance_proxy), 50, 1},
  {Op::define_molecule, 102, static_cast<std::uint32_t>(MoleculeKind::rna_proxy), 70, 1},
  {Op::connect, 1, 3, 15},
  {Op::teach_symbol, 3, 77, 30},
  {Op::advance, 0, 0, 12},
  {Op::observe}
}};

static void run_program(Sim& sim) {
  for (const auto& instruction : kProgram)
    CHECK(sim.apply(instruction) == Status::ok);
}

static void deterministic_replay_and_movement() {
  Sim left{}, right{};
  run_program(left); run_program(right);
  CHECK(left.state_hash() == right.state_hash());
  CHECK(left.event_chain() == right.event_chain());
  CHECK(left.event_count() == kProgram.size());
  auto m = left.metrics();
  CHECK(m.tick == 12 && m.cells == 5 && m.viable_cells == 5);
  CHECK(m.links == 1 && m.molecules == 3);
  CHECK(m.rna_proxy > 0 && m.neurite_proxy > 0 && m.learning_proxy == 30);
  CHECK(left.cell(2)->x == 15); // synthetic moving blood carrier
  CHECK(left.cell(1)->kind == CellKind::peripheral_neuron);
  CHECK(left.cell(3)->kind == CellKind::central_neuron);
  CHECK(left.cell(5)->kind == CellKind::astrocyte);
  CHECK(left.cell(3)->last_symbol == 77);
  CHECK(left.event(0)->parent_hash == kFnvOffset);
  for (std::size_t i = 1; i < left.event_count(); ++i) {
    CHECK(left.event(i)->sequence == i + 1);
    CHECK(left.event(i)->tick >= left.event(i - 1)->tick);
    CHECK(left.event(i)->state_hash != 0);
  }
  Sim checkpoint = left; // entire bounded simulator including evidence history
  CHECK(checkpoint.state_hash() == left.state_hash());
  CHECK(checkpoint.apply({Op::move, 1, 0, 2, -1}) == Status::ok);
  CHECK(checkpoint.state_hash() != left.state_hash());
  CHECK(left.cell(1)->x == 10); // original snapshot unchanged
  CHECK(right.apply({Op::move, 1, 0, 2, -1}) == Status::ok);
  CHECK(checkpoint.state_hash() == right.state_hash());
  CHECK(checkpoint.event_chain() == right.event_chain());
}

static void rejected_instructions_are_atomic() {
  Sim sim{};
  run_program(sim);
  const auto hash = sim.state_hash();
  const auto count = sim.event_count();
  CHECK(sim.apply({Op::differentiate, 2, static_cast<std::uint32_t>(CellKind::central_neuron)}) == Status::illegal_lineage);
  CHECK(sim.apply({Op::connect, 2, 3, 20}) == Status::incompatible_connection);
  CHECK(sim.apply({Op::spawn, 1, static_cast<std::uint32_t>(CellKind::central_neuron)}) == Status::duplicate_id);
  CHECK(sim.apply({Op::move, 1, 0, 9000, 0}) == Status::out_of_bounds);
  CHECK(sim.apply({Op::spawn, 99, static_cast<std::uint32_t>(CellKind::central_neuron),
                  20, 20, 2}) == Status::illegal_lineage);

  CHECK(sim.apply({Op::define_molecule, 103, 254, 20}) == Status::invalid_kind);
  CHECK(sim.apply({Op::teach_symbol, 5, 77, 20}) == Status::invalid_kind);
  CHECK(sim.apply({Op::supply, 999, 0, 2}) == Status::missing_definition);
  CHECK(sim.apply({Op::advance, 0, 0, 0}) == Status::overflow);
  CHECK(sim.event_count() == count);
  CHECK(sim.state_hash() == hash);
}

static void molecule_inventory_and_capacity() {
  Simulator<1, 1, 1, 2> sim;
  CHECK(sim.apply({Op::define_molecule, 5, static_cast<std::uint32_t>(MoleculeKind::support), 3, 1}) == Status::ok);
  CHECK(sim.apply({Op::define_molecule, 6, static_cast<std::uint32_t>(MoleculeKind::waste_proxy), 3}) == Status::capacity_exceeded);
  CHECK(sim.apply({Op::supply, 5, 0, 100000}) == Status::ok);
  CHECK(sim.molecule(5)->quantity == 100000);
  auto hash = sim.state_hash();
  CHECK(sim.apply({Op::observe}) == Status::capacity_exceeded);
  CHECK(sim.state_hash() == hash);
}

struct Bridge {
  BoundedScheduler<32> scheduler{};
  std::size_t encoded = 0;
  std::size_t errors = 0;
  std::uint64_t last_digest = 0;
  static void push(void* opaque, const Evidence& evidence) noexcept {
    auto& self = *static_cast<Bridge*>(opaque);
    Header h{};
    h.kind = Kind::observation;
    h.delivery_class = DeliveryClass::evidence;
    h.stream_id = 12;
    h.sequence = evidence.sequence;
    h.source_slot = 100;
    h.destination_slot = 200;
    h.schema_id = 0xCE600001u; // local synthetic simulation-envelope schema
    h.payload_bytes = 8;
    std::array<std::byte, 8> payload{};
    for (std::size_t i = 0; i < payload.size(); ++i)
      payload[i] = std::byte((evidence.state_hash >> (8 * i)) & 0xffu);
    std::array<std::byte, 64> bytes{};
    std::size_t written = 0;
    View decoded{};
    if (encode(h, {}, payload, bytes, written) != Error::ok ||
        decode(std::span<const std::byte>(bytes.data(), written), decoded) != Error::ok ||
        decoded.header.sequence != evidence.sequence ||
        self.scheduler.post(DeliveryClass::evidence, {evidence.sequence, 0}) != PostResult::accepted) {
      ++self.errors;
      return;
    }
    self.last_digest = evidence.state_hash;
    ++self.encoded;
  }
};

static void bytecode_instruction_set_roundtrip() {
  std::array<std::byte, 32 * kProgram.size()> stream{};
  for (std::size_t i = 0; i < kProgram.size(); ++i) {
    auto block = std::span<std::byte>(stream).subspan(i * 32, 32);
    CHECK(encode_instruction(kProgram[i], block) == BytecodeError::ok);
  }
  CHECK(stream[0] == std::byte{'C'} && stream[3] == std::byte{'P'});
  CHECK(stream[4] == std::byte{1});
  Sim decoded{}, original{};
  run_program(original);
  auto result = christeye::living::run_program(decoded, stream);
  CHECK(result.wire_error == BytecodeError::ok);
  CHECK(result.simulation_status == Status::ok);
  CHECK(result.processed == kProgram.size());
  CHECK(decoded.state_hash() == original.state_hash());
  CHECK(decoded.event_chain() == original.event_chain());
  const auto intact = decoded.state_hash();
  stream[0] = std::byte{0};
  result = christeye::living::run_program(decoded, stream);
  CHECK(result.wire_error == BytecodeError::bad_magic);
  CHECK(result.processed == 0);
  CHECK(decoded.state_hash() == intact);
  stream[0] = std::byte{'C'};
  stream[6] = std::byte{1};
  Instruction nope{};
  CHECK(decode_instruction(std::span<const std::byte>(stream).first(32), nope) ==
        BytecodeError::reserved_nonzero);
  stream[6] = std::byte{0};
  CHECK(christeye::living::run_program(decoded,
        std::span<const std::byte>(stream).first(stream.size()-1)).wire_error ==
        BytecodeError::bad_size);
}

static void windbus_integration() {
  Sim sim;
  Bridge bridge;
  sim.set_evidence_callback(Bridge::push, &bridge);
  run_program(sim);
  CHECK(bridge.errors == 0);
  CHECK(bridge.encoded == kProgram.size());
  CHECK(bridge.last_digest == sim.state_hash());
  std::uint64_t expected = 1;
  while (bridge.scheduler.pending()) {
    auto work = bridge.scheduler.next(1);
    CHECK(work.status == TakeStatus::ready);
    CHECK(work.delivery_class == DeliveryClass::evidence);
    CHECK(work.item.token == expected++);
  }
  CHECK(expected == kProgram.size() + 1);
}
int main() {
  deterministic_replay_and_movement();
  rejected_instructions_are_atomic();
  molecule_inventory_and_capacity();
  bytecode_instruction_set_roundtrip();
  windbus_integration();
  std::cout << "SIM1 deterministic living surrogate and WindBus integration passed\n";
}
