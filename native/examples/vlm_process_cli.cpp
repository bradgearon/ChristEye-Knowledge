// CEVLM/2 deterministic process orchestration / animated timeline CLI.
#include "christeye/living/process_machine.hpp"
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <string_view>
#include <vector>
namespace p = christeye::living::process;
namespace v = christeye::living::vlm;

static int fail(std::string_view what, p::Error e, std::size_t line = 0) {
  std::cerr << what << " failed, status=" << static_cast<unsigned>(e) << " line=" << line << '\n';
  return 1;
}
static void show(const p::ProcessMachine& m, bool full) {
  const auto& metrics = m.base().core().metrics();
  std::cout << "{\"classification\":\"SIMULATED_UNCALIBRATED\",\"format\":\"CEVLM/2\""
            << ",\"machine\":\"" << m.base().name() << "\",\"tick\":" << m.tick()
            << ",\"cells\":" << metrics.cells
            << ",\"molecule_types\":" << metrics.molecules
            << ",\"neurite_proxy\":" << metrics.neurite_proxy
            << ",\"rna_proxy\":" << metrics.rna_proxy
            << ",\"learning_proxy\":" << metrics.learning_proxy
            << ",\"state_hash\":\"" << v::hex64(m.state_hash()) << "\"";
  std::cout << ",\"fields\":{";
  for (std::uint32_t kind = 0; kind < v::kFieldTypes; ++kind) {
    if (kind) std::cout << ',';
    const auto f = static_cast<v::FieldKind>(kind);
    std::cout << '"' << v::field_name(f) << "\":" << m.base().soil().total(f);
  }
  std::cout << "},\"processes\":[";
  bool first = true;
  for (const auto& task : m.tasks()) {
    if (!first) std::cout << ',';
    first = false;
    const auto& def = m.definitions()[task.definition];
    const auto* asset = m.asset(def.asset_id);
    std::cout << "{\"id\":" << task.id << ",\"process\":\"" << def.id
              << "\",\"operation\":\"" << def.operation
              << "\",\"asset\":\"" << (asset ? asset->path : "")
              << "\",\"asset_sha256\":\"" << (asset ? asset->sha256 : "")
              << "\",\"asset_role\":\"" << (asset ? asset->role : "")
              << "\",\"duration\":" << def.duration << ",\"elapsed\":" << task.elapsed
              << ",\"weight\":" << def.weight << ",\"x\":" << task.x
              << ",\"y\":" << task.y << ",\"completed\":" << (task.completed ? "true" : "false") << '}';
  }
  std::cout << ']';
  if (full) {
    std::set<std::uint32_t> ids;
    const auto collect = [&](const v::Action& a) {
      if (!a.is_deposit && a.instruction.op == christeye::living::Op::spawn)
        ids.insert(a.instruction.subject);
    };
    for (const auto& a : m.base().image()) collect(a);
    for (const auto& a : m.base().journal()) collect(a);
    std::cout << ",\"actors\":[";
    bool first_actor = true;
    for (auto id : ids) {
      const auto* c = m.base().core().cell(id);
      if (!c) continue;
      if (!first_actor) std::cout << ',';
      first_actor = false;
      std::cout << "{\"id\":" << c->id << ",\"kind\":\""
                << v::cell_name(c->kind) << "\",\"x\":" << c->x
                << ",\"y\":" << c->y << ",\"energy_proxy\":" << c->energy
                << ",\"neurite_proxy\":" << c->neurite_proxy
                << ",\"rna_proxy\":" << c->rna_proxy << '}';
    }
    std::cout << "],\"connections\":[";
    for (std::size_t index = 0; index < m.base().core().connection_count(); ++index) {
      if (index) std::cout << ',';
      const auto* c = m.base().core().connection(index);
      if (c) std::cout << "{\"from\":" << c->source << ",\"to\":" << c->target
                       << ",\"weight\":" << c->weight << '}';
    }
    std::cout << "],\"field_tiles\":{";
    for (std::uint32_t field = 0; field < v::kFieldTypes; ++field) {
      if (field) std::cout << ',';
      const auto f = static_cast<v::FieldKind>(field);
      std::cout << '"' << v::field_name(f) << "\":[";
      for (std::uint32_t y = 0; y < v::kFieldSide; ++y) {
        if (y) std::cout << ',';
        std::cout << '[';
        for (std::uint32_t x = 0; x < v::kFieldSide; ++x) {
          if (x) std::cout << ',';
          std::cout << m.base().soil().at(f, x, y);
        }
        std::cout << ']';
      }
      std::cout << ']';
    }
    std::cout << "},\"events\":[";
    bool started = false;
    for (const auto& e : m.events()) {
      if (e.tick != m.tick()) continue;
      if (started) std::cout << ',';
      started = true;
      std::cout << "{\"task\":" << e.task_id << ",\"weight\":" << e.weight
                << ",\"progress\":" << e.elapsed << ",\"x\":" << e.x
                << ",\"y\":" << e.y << ",\"completed\":" << (e.completed ? "true" : "false") << '}';
    }
    std::cout << ']';
  }
  std::cout << "}\n";
}
static int usage() {
  std::cerr << "CEVLM/2 commands:\n"
            << "  ce_vlm_process inspect <file.avlm>\n"
            << "  ce_vlm_process verify <sealed.avlm>\n"
            << "  ce_vlm_process new <source.avlm> <dest.avlm> <name>\n"
            << "  ce_vlm_process fork <source.avlm> <dest.avlm> <name>\n"
            << "  ce_vlm_process run <source.avlm> <dest.avlm> <ticks 1..512>\n"
            << "  ce_vlm_process start <source.avlm> <dest.avlm> <task-id> <process-id> <action...>\n"
            << "  ce_vlm_process stream <source.avlm> <ticks 1..512>  # NDJSON\n"
            << "  ce_vlm_process upgrade <legacy1.avlm> <catalog2.avlm> <out2.avlm> <name>\n"
            << "  ce_vlm_process execute <source2.avlm> <out2.avlm> <action...>\n";
  return 2;
}
int main(int argc, char** argv) {
  if (argc < 3) return usage();
  const std::string_view cmd(argv[1]);
  if (cmd == "upgrade") {
    if (argc != 6) return usage();
    v::VirtualMachine old;
    const auto legacy = v::read_file(argv[2], old);
    if (legacy.result != v::Result::ok) {
      std::cerr << "legacy load failed: " << static_cast<unsigned>(legacy.result) << '\n';
      return 1;
    }
    p::ProcessMachine catalog, output;
    if (const auto r = p::read_file(argv[3], catalog); r.error != p::Error::ok)
      return fail("catalog", r.error, r.line);
    if (auto r = p::ProcessMachine::import_v1(old, catalog, output, argv[5]);
        r != p::Error::ok) return fail("upgrade", r);
    if (auto r = p::write_file(argv[4], output); r != p::Error::ok) return fail("write", r);
    show(output, true);
    return 0;
  }
  p::ProcessMachine machine;
  const auto loaded = p::read_file(argv[2], machine, cmd == "verify");
  if (loaded.error != p::Error::ok) return fail("load", loaded.error, loaded.line);
  if (cmd == "inspect" || cmd == "verify") {
    if (argc != 3) return usage();
    show(machine, true);
    return 0;
  }
  if (cmd == "new" || cmd == "fork") {
    if (argc != 5) return usage();
    p::ProcessMachine target;
    if (cmd == "new") {
      if (auto r = machine.clone_image_to(target, argv[4]); r != p::Error::ok)
        return fail("new", r);
    } else {
      target = machine;
      if (!target.name(argv[4])) return fail("fork", p::Error::invalid_task);
    }
    if (auto r = p::write_file(argv[3], target); r != p::Error::ok) return fail("write", r);
    show(target, true);
    return 0;
  }
  if (cmd == "run" || cmd == "stream") {
    if (argc != (cmd == "run" ? 5 : 4)) return usage();
    std::int32_t n = 0;
    if (!v::parse_signed(argv[cmd == "run" ? 4 : 3], n) || n < 1 || n > 512)
      return fail("ticks", p::Error::invalid_task);
    for (std::int32_t i = 0; i < n; ++i) {
      if (auto r = machine.advance(1); r != p::Error::ok) return fail("advance", r);
      if (cmd == "stream") show(machine, true);
    }
    if (cmd == "stream") return 0;
    if (auto r = p::write_file(argv[3], machine); r != p::Error::ok) return fail("write", r);
    show(machine, true);
    return 0;
  }
  if (cmd == "execute") {
    if (argc < 6) return usage();
    std::string action;
    for (int i = 4; i < argc; ++i) { if (i != 4) action += ' '; action += argv[i]; }
    v::Action a;
    if (!v::parse_action(action, a)) return fail("parse action", p::Error::malformed);
    if (auto r = machine.execute(a); r != p::Error::ok) return fail("execute", r);
    if (auto r = p::write_file(argv[3], machine); r != p::Error::ok) return fail("write", r);
    show(machine, true);
    return 0;
  }
  if (cmd == "start") {
    if (argc < 8) return usage();
    std::uint32_t id = 0;
    if (!v::parse_unsigned(argv[4], id)) return fail("task id", p::Error::invalid_task);
    std::string action;
    for (int i = 6; i < argc; ++i) { if (i != 6) action += ' '; action += argv[i]; }
    v::Action a;
    if (!v::parse_action(action, a)) return fail("parse action", p::Error::malformed);
    if (auto r = machine.start(id, argv[5], a); r != p::Error::ok) return fail("start", r);
    if (auto r = p::write_file(argv[3], machine); r != p::Error::ok) return fail("write", r);
    show(machine, true);
    return 0;
  }
  return usage();
}
