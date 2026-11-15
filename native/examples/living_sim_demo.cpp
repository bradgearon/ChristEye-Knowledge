// Digital-only Anaiero SIM1: a finite moving colony visualizable as NDJSON.
// Every line is a snapshot *after* an executed synthetic instruction tick.
#include "christeye/living/simulator.hpp"
#include <array>
#include <cstdint>
#include <iostream>
using namespace christeye::living;

static void render(const Simulator<32, 8, 32, 128>& c) {
  const auto m = c.metrics();
  const auto* crest_neuron = c.cell(1);
  const auto* blood = c.cell(2);
  const auto* central_neuron = c.cell(3);
  std::cout << "{\"model\":\"SIM1_UNCALIBRATED_DIGITAL\",\"tick\":" << m.tick
            << ",\"cells\":" << m.cells << ",\"edges\":" << m.links
            << ",\"rna_proxy\":" << m.rna_proxy
            << ",\"neurite_proxy\":" << m.neurite_proxy
            << ",\"total_energy\":" << m.total_energy
            << ",\"agents\":["
            << "{\"id\":1,\"kind\":\"peripheral_neuron\",\"x\":" << crest_neuron->x
            << ",\"y\":" << crest_neuron->y << "},"
            << "{\"id\":2,\"kind\":\"blood_carrier\",\"x\":" << blood->x
            << ",\"y\":" << blood->y << "},"
            << "{\"id\":3,\"kind\":\"central_neuron\",\"x\":" << central_neuron->x
            << ",\"y\":" << central_neuron->y << "}],"
            << "\"state_hash\":" << c.state_hash()
            << ",\"event_count\":" << c.event_count() << "}\n";
}
int main() {
  Simulator<32, 8, 32, 128> colony;
  const std::array<Instruction, 9> instructions{{
    {Op::spawn, 1, static_cast<std::uint32_t>(CellKind::crest_progenitor), 2, 1},
    {Op::spawn, 2, static_cast<std::uint32_t>(CellKind::blood_carrier), 8, 3},
    {Op::spawn, 3, static_cast<std::uint32_t>(CellKind::neural_progenitor), 5, 4},
    {Op::differentiate, 1, static_cast<std::uint32_t>(CellKind::peripheral_neuron)},
    {Op::differentiate, 3, static_cast<std::uint32_t>(CellKind::central_neuron)},
    {Op::define_molecule, 1, static_cast<std::uint32_t>(MoleculeKind::support), 40, 1},
    {Op::define_molecule, 2, static_cast<std::uint32_t>(MoleculeKind::rna_proxy), 30, 1},
    {Op::connect, 1, 3, 20},
    {Op::teach_symbol, 3, 18, 20}
  }};
  for (auto op : instructions) {
    if (colony.apply(op) != Status::ok) return 1;
  }
  for (int i = 0; i < 10; ++i) {
    if (colony.apply({Op::advance, 0, 0, 1}) != Status::ok) return 2;
    render(colony);
  }
}
