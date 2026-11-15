// Anaiero VLM1 host CLI: one-file digital living-machine image + runtime journal.
#include "christeye/living/virtual_machine.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace vlm = christeye::living::vlm;
namespace living = christeye::living;

static int failed(std::string_view action, vlm::LoadResult result) {
  std::cerr << action << " failed: error=" << static_cast<unsigned>(result.result)
            << " line=" << result.line << "\n";
  return 1;
}
static void print_state(const vlm::VirtualMachine& m, bool detailed = false) {
  const auto state = m.core().metrics();
  std::cout << "{\"class\":\"SIMULATED_UNCALIBRATED\",\"machine\":\"" << m.name()
            << "\",\"tick\":" << state.tick << ",\"cells\":" << state.cells
            << ",\"viable_cells\":" << state.viable_cells
            << ",\"molecule_types\":" << state.molecules
            << ",\"links\":" << state.links << ",\"energy_proxy\":" << state.total_energy
            << ",\"neurite_proxy\":" << state.neurite_proxy
            << ",\"rna_proxy\":" << state.rna_proxy
            << ",\"learning_proxy\":" << state.learning_proxy
            << ",\"support_field\":" << m.soil().total(vlm::FieldKind::support)
            << ",\"soil_field\":" << m.soil().total(vlm::FieldKind::soil)
            << ",\"state_hash\":\"" << vlm::hex64(m.state_hash()) << "\""
            << ",\"action_chain\":\"" << vlm::hex64(m.action_chain()) << "\"";
  if (const auto* cell = m.core().cell(2))
    std::cout << ",\"blood_carrier_2\":{\"x\":" << cell->x
              << ",\"y\":" << cell->y << "}";
  if (detailed) {
    std::cout << ",\"actors\":[";
    bool first = true;
    const auto present = [&](const vlm::Action& action) {
      if (action.is_deposit || action.instruction.op != living::Op::spawn) return;
      const auto* c = m.core().cell(action.instruction.subject);
      if (!c) return;
      if (!first) std::cout << ',';
      first = false;
      std::cout << "{\"id\":" << c->id << ",\"kind\":\""
                << vlm::cell_name(c->kind) << "\",\"x\":" << c->x
                << ",\"y\":" << c->y << ",\"maturity\":" << c->maturity
                << ",\"neurite_proxy\":" << c->neurite_proxy
                << ",\"rna_proxy\":" << c->rna_proxy
                << ",\"learning_proxy\":" << c->learning_proxy << '}';
    };
    for (const auto& a : m.image()) present(a);
    for (const auto& a : m.journal()) present(a);
    std::cout << "],\"fields\":{";
    for (unsigned f = 0; f < vlm::kFieldTypes; ++f) {
      if (f != 0) std::cout << ',';
      const auto kind = static_cast<vlm::FieldKind>(f);
      std::cout << '"' << vlm::field_name(kind) << "\":[";
      for (unsigned y = 0; y < vlm::kFieldSide; ++y) {
        if (y != 0) std::cout << ',';
        std::cout << '[';
        for (unsigned x = 0; x < vlm::kFieldSide; ++x) {
          if (x != 0) std::cout << ',';
          std::cout << m.soil().at(kind, x, y);
        }
        std::cout << ']';
      }
      std::cout << ']';
    }
    std::cout << '}';
  }
  std::cout << "}\n";
}
static int usage() {
  std::cerr << "Commands:\n"
            << "  ce_vlm inspect <setup-or-bundle.avlm>\n"
            << "  ce_vlm verify <sealed-bundle.avlm>\n"
            << "  ce_vlm new <existing.avlm> <new.avlm> <machine-name>  # image only\n"
            << "  ce_vlm fork <running.avlm> <fork.avlm> <machine-name> # preserve state\n"
            << "  ce_vlm run <input.avlm> <output.avlm> <ticks>\n"
            << "  ce_vlm execute <input.avlm> <ops.txt> <output.avlm>\n"
            << "  ce_vlm stream <input.avlm> <ticks>     # NDJSON motion frames\n";
  return 2;
}
int main(int argc, char** argv) {
  if (argc < 3) return usage();
  const std::string_view cmd(argv[1]);
  vlm::VirtualMachine m;
  const bool must_be_sealed = cmd == "verify";
  const auto loaded = vlm::read_file(argv[2], m, must_be_sealed);
  if (loaded.result != vlm::Result::ok) return failed("load", loaded);
  if (cmd == "inspect" || cmd == "verify") {
    if (argc != 3) return usage();
    print_state(m);
    return 0;
  }
  if (cmd == "new" || cmd == "fork") {
    if (argc != 5) return usage();
    vlm::VirtualMachine target;
    if (cmd == "new") {
      const auto result = m.clone_image_to(target, argv[4]);
      if (result != vlm::Result::ok) return failed("clone image", {result, 0});
    } else {
      target = m;
      if (!target.set_name(argv[4])) return failed("rename", {vlm::Result::invalid_setup, 0});
    }
    if (vlm::write_file(argv[3], target) != vlm::Result::ok)
      return failed("save", {vlm::Result::io_error, 0});
    print_state(target);
    return 0;
  }
  if (cmd == "run" || cmd == "stream") {
    if (argc != (cmd == "run" ? 5 : 4)) return usage();
    std::int32_t ticks = 0;
    const std::string_view value(argv[cmd == "run" ? 4 : 3]);
    if (!vlm::parse_signed(value, ticks) || ticks < 1 || ticks > 10000)
      return failed("bad tick count", {vlm::Result::invalid_action, 0});
    if (cmd == "stream") {
      for (std::int32_t i = 0; i < ticks; ++i) {
        if (m.advance(1) != vlm::Result::ok) return failed("advance", {vlm::Result::full, 0});
        print_state(m, true);
      }
      return 0;
    }
    if (m.advance(ticks) != vlm::Result::ok) return failed("advance", {vlm::Result::full, 0});
    if (vlm::write_file(argv[3], m) != vlm::Result::ok)
      return failed("save", {vlm::Result::io_error, 0});
    print_state(m);
    return 0;
  }
  if (cmd == "execute") {
    if (argc != 5) return usage();
    std::ifstream instructions(argv[3]);
    if (!instructions) return failed("read operations", {vlm::Result::io_error, 0});
    std::string line;
    std::size_t n = 0;
    // Transactional at file level: intermediate state exists only in memory.
    while (std::getline(instructions, line)) {
      ++n;
      if (line.empty() || line[0] == '#') continue;
      vlm::Action action;
      if (!vlm::parse_action(line, action)) return failed("parse operation", {vlm::Result::malformed_line, n});
      const auto r = m.apply(action);
      if (r != vlm::Result::ok) return failed("execute operation", {r, n});
    }
    if (vlm::write_file(argv[4], m) != vlm::Result::ok)
      return failed("save", {vlm::Result::io_error, 0});
    print_state(m);
    return 0;
  }
  return usage();
}
