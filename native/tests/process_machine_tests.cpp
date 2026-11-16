#include "christeye/living/process_machine.hpp"
#include "christeye/wind/packet.hpp"
#include "christeye/wind/scheduler.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
namespace p = christeye::living::process;
namespace v = christeye::living::vlm;
namespace wind = christeye::wind;
#define EXPECT(cond) do { if(!(cond)){std::cerr << "FAIL " #cond " line " << __LINE__ << '\n'; std::exit(1);}}while(false)
static const char* scenario = R"(
CEVLM/2
name test-garden
model 1
assets
asset crest-glow sprite docs/assets/vlm/crest-glow.svg a6ce313a3247ba73a8ccb5012bcb3283af7716ce64dccaf00548f8495445ed0d
asset molecule-signal icon docs/assets/vlm/molecule-signal.svg df08bcfae0ee04a83ffabd176033f1c234f83e560605830a5d1c31ba79418991
asset support-flow heatmap docs/assets/vlm/support-flow.svg 0f29e943cd260248e02046577f1591b83448f82d1ffc733b5ebcae5d54a3eceb
endassets
processes
process birth spawn 4 850 1 0 crest-glow
process signal molecule 2 350 0 0 molecule-signal
process food deposit 4 750 1 0 support-flow
endprocesses
image
spawn 1 neural_progenitor 10 12 0
differentiate 1 central_neuron
molecule 100 support 80 1
endimage
journal
start 11 birth spawn 10 crest_progenitor 10 12 0
start 12 food deposit support 2 2 400
start 13 signal molecule 101 protein_proxy 80 1
advance 2
endjournal
)";
static p::ProcessMachine make() {
  p::ProcessMachine a;
  EXPECT(p::ProcessMachine::deserialize(scenario, a).error == p::Error::ok);
  EXPECT(a.base().core().metrics().cells == 1);
  EXPECT(a.tick() == 2 && a.tasks().size() == 3);
  EXPECT(!a.base().core().cell(10));
  EXPECT(a.base().core().molecule(101));
  EXPECT(a.base().soil().total(v::FieldKind::support) == 0);
  return a;
}
static void schedule_and_save() {
  auto a = make();
  EXPECT(a.events().size() == 6);
  EXPECT(a.events()[0].task_id == 11);
  EXPECT(a.events()[1].task_id == 12);
  EXPECT(a.events()[2].task_id == 13);
  EXPECT(a.tasks()[2].completed);
  EXPECT(a.advance(2) == p::Error::ok);
  EXPECT(a.tick() == 4);
  const auto* crest = a.base().core().cell(10);
  EXPECT(crest && crest->x == 14 && crest->y == 12);
  EXPECT(a.base().soil().at(v::FieldKind::support, 6, 2) == 400);
  EXPECT(a.tasks()[0].completed && a.tasks()[1].completed);
  EXPECT(a.events().back().task_id == 12);
  const auto sealed = a.serialize();
  p::ProcessMachine b;
  EXPECT(p::ProcessMachine::deserialize(sealed, b, true).error == p::Error::ok);
  EXPECT(b.serialize() == sealed);
  EXPECT(b.state_hash() == a.state_hash());
  EXPECT(b.events().size() == a.events().size());
  EXPECT(b.advance(5) == p::Error::ok && a.advance(5) == p::Error::ok);
  EXPECT(b.state_hash() == a.state_hash());
  EXPECT(b.base().soil().total(v::FieldKind::support) == 400);
  const auto file = std::filesystem::temp_directory_path() / "cevlm2-process-test.avlm";
  EXPECT(p::write_file(file, b) == p::Error::ok);
  p::ProcessMachine disk;
  EXPECT(p::read_file(file, disk, true).error == p::Error::ok);
  EXPECT(disk.state_hash() == b.state_hash());
  std::error_code ec;
  std::filesystem::remove(file, ec);
}
static void clones_and_negative_cases() {
  auto a = make();
  const auto hash = a.state_hash();
  p::ProcessMachine next;
  EXPECT(a.clone_image_to(next, "new-garden") == p::Error::ok);
  EXPECT(next.tick() == 0 && next.tasks().empty() && next.definitions().size() == 3);
  auto fork = a;
  EXPECT(fork.name("branch-garden"));
  EXPECT(fork.advance(1) == p::Error::ok && a.state_hash() == hash);
  EXPECT(fork.state_hash() != a.state_hash());
  p::ProcessMachine target = make();
  auto bad = target.serialize();
  bad[bad.find("asset crest-glow") + 6] = 'X';
  EXPECT(p::ProcessMachine::deserialize(bad, target, true).error != p::Error::ok);
  EXPECT(target.state_hash() == hash);
  bad = target.serialize();
  auto idx = bad.find("seal ");
  EXPECT(idx != std::string::npos);
  bad[idx + 7] = '0';
  EXPECT(p::ProcessMachine::deserialize(bad, target, true).error == p::Error::checksum);
  EXPECT(target.state_hash() == hash);
  bad = target.serialize(false);
  idx = bad.find("process birth");
  bad.replace(idx, 13, "process nope");
  EXPECT(p::ProcessMachine::deserialize(bad, target).error != p::Error::ok);
  EXPECT(target.state_hash() == hash);
  EXPECT(!p::asset_path("../secrets.svg"));
  EXPECT(!p::asset_path("https://host/evil.svg"));
  EXPECT(!p::asset_path("docs/assets/../../private.svg"));
  EXPECT(!p::asset_path("evil.html"));
  EXPECT(p::asset_path("docs/assets/vlm/icon.svg"));
  v::Action other{};
  EXPECT(v::parse_action("spawn 99 crest_progenitor 10 10 0", other));
  EXPECT(a.start(13, "birth", other) == p::Error::duplicate_id);
}
static void failed_completion_is_transactional() {
  p::ProcessMachine a = make();
  v::Action duplicate{};
  EXPECT(v::parse_action("spawn 1 crest_progenitor 20 20 0", duplicate));
  EXPECT(a.start(70, "birth", duplicate) == p::Error::ok);
  EXPECT(a.advance(2) == p::Error::ok);
  const auto state = a.state_hash();
  EXPECT(a.advance(2) == p::Error::execution_failed); // already-existing cell 1
  EXPECT(a.state_hash() == state && a.tick() == 4);
}
static void import_v1_and_manual_ops() {
  const std::string old_text =
      "CEVLM/1\nname former-garden\nmodel 1\nimage\n"
      "spawn 1 neural_progenitor 10 12 0\n"
      "differentiate 1 central_neuron\n"
      "molecule 100 support 80 1\n"
      "deposit support 1 1 100\n"
      "endimage\njournal\nadvance 3\nteach 1 42 12\nendjournal\n";
  v::VirtualMachine old;
  EXPECT(v::VirtualMachine::deserialize(old_text, old).result == v::Result::ok);
  p::ProcessMachine library = make();
  p::ProcessMachine converted;
  EXPECT(p::ProcessMachine::import_v1(old, library, converted, "upgraded") == p::Error::ok);
  EXPECT(converted.definitions().size() == library.definitions().size());
  EXPECT(converted.tick() == old.core().tick());
  EXPECT(converted.base().core().cell(1)->learning_proxy == old.core().cell(1)->learning_proxy);
  EXPECT(converted.base().soil().total(v::FieldKind::support) == old.soil().total(v::FieldKind::support));
  EXPECT(converted.journal().size() == 2); // one replayable advance + one direct lesson
  EXPECT(converted.journal().back().direct);
  p::ProcessMachine restored;
  EXPECT(p::ProcessMachine::deserialize(converted.serialize(), restored, true).error == p::Error::ok);
  EXPECT(restored.state_hash() == converted.state_hash());
  v::Action action;
  EXPECT(v::parse_action("deposit guidance 2 2 30", action));
  EXPECT(restored.execute(action) == p::Error::ok);
  EXPECT(restored.base().soil().total(v::FieldKind::guidance) == 30);
  EXPECT(restored.state_hash() != converted.state_hash());
}
static void invalid_process_config_is_rejected() {
  p::ProcessMachine machine = make();
  const auto baseline = machine.state_hash();
  auto source = std::string(scenario);
  const auto pos = source.find("process birth spawn 4 850");
  EXPECT(pos != std::string::npos);
  source.replace(pos, std::string("process birth spawn 4 850").size(), "process birth spawn 0 850");
  EXPECT(p::ProcessMachine::deserialize(source, machine).error != p::Error::ok);
  EXPECT(machine.state_hash() == baseline);
  source = scenario;
  auto asset = source.find("docs/assets/vlm/crest-glow.svg");
  EXPECT(asset != std::string::npos);
  source.replace(asset, std::string("docs/assets/vlm/crest-glow.svg").size(), "../private.svg");
  EXPECT(p::ProcessMachine::deserialize(source, machine).error != p::Error::ok);
  EXPECT(machine.state_hash() == baseline);
  source = scenario;
  const auto section = source.find("endassets\nprocesses\n");
  EXPECT(section != std::string::npos);
  source.replace(section, std::string("endassets\nprocesses\n").size(), "endassets\n");
  EXPECT(p::ProcessMachine::deserialize(source, machine).error != p::Error::ok);
  EXPECT(machine.state_hash() == baseline);
}
struct PushCapture {
  std::uint32_t calls = 0;
  std::uint64_t newest_tick = 0;
  bool bad = false;
  static void delivered(void* opaque, const p::Event& event) noexcept {
    auto& self = *static_cast<PushCapture*>(opaque);
    ++self.calls;
    if (event.tick < 3 || event.snapshot_hash == 0) self.bad = true;
    self.newest_tick = event.tick;
  }
};
static void post_commit_events_are_not_replayed_on_failure() {
  auto a = make();
  PushCapture receiver;
  a.set_process_event_callback(PushCapture::delivered, &receiver);
  EXPECT(a.advance(2) == p::Error::ok);
  EXPECT(receiver.calls == 4 && receiver.newest_tick == 4 && !receiver.bad);
  v::Action duplicate{};
  EXPECT(v::parse_action("spawn 1 crest_progenitor 20 20 0", duplicate));
  EXPECT(a.start(70, "birth", duplicate) == p::Error::ok);
  EXPECT(a.advance(2) == p::Error::ok);
  const auto prior = receiver.calls;
  EXPECT(a.advance(2) == p::Error::execution_failed);
  EXPECT(receiver.calls == prior); // Nothing leaks from rolled-back transaction.
}
static void windpacket_integration() {
  auto a = make();
  wind::BoundedScheduler<32> queue;
  std::uint64_t seq = 1;
  for (const auto& event : a.events()) {
    wind::Header header{};
    header.kind = wind::Kind::observation;
    header.delivery_class = wind::DeliveryClass::evidence;
    header.sequence = seq++;
    header.stream_id = 44;
    header.schema_id = 0xCE600008u;
    header.payload_bytes = 16;
    std::array<std::byte, 16> payload{};
    for (unsigned i = 0; i < 8; ++i) {
      payload[i] = std::byte((static_cast<std::uint64_t>(event.task_id) >> (8u*i)) & 0xffu);
      payload[i+8] = std::byte((event.tick >> (8u*i)) & 0xffu);
    }
    std::array<std::byte, 72> frame{};
    std::size_t written = 0;
    wind::View decoded{};
    EXPECT(wind::encode(header, {}, payload, frame, written) == wind::Error::ok);
    EXPECT(wind::decode(std::span<const std::byte>(frame.data(), written), decoded) == wind::Error::ok);
    EXPECT(decoded.header.sequence == header.sequence && decoded.payload.size() == 16);
    EXPECT(queue.post(wind::DeliveryClass::evidence, {header.sequence, 0}) == wind::PostResult::accepted);
  }
  EXPECT(queue.pending() == a.events().size());
  for (std::uint64_t n = 1; n < seq; ++n) {
    auto item = queue.next(0);
    EXPECT(item.status == wind::TakeStatus::ready && item.item.token == n);
  }
  EXPECT(queue.pending() == 0);
}
int main() {
  schedule_and_save(); clones_and_negative_cases();
  failed_completion_is_transactional(); import_v1_and_manual_ops();
  invalid_process_config_is_rejected(); post_commit_events_are_not_replayed_on_failure();
  windpacket_integration();
  std::cout << "VLM2 process catalogue, timeline, replay and WindBus tests passed\n";
}
