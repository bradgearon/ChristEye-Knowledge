#include "christeye/wind/media_patchbay.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
using namespace christeye::wind;
#define CHECK(test) do { if (!(test)) { std::cerr << "FAIL: " #test << " at " << __LINE__ << "\n"; std::exit(1); } } while (false)

struct Receiver {
  std::uint32_t last_port = 0;
  std::uint64_t last_sample = 0;
  float sum = 0;
  int pcm_calls = 0;
  int midi_calls = 0;
  std::uint8_t midi_words = 0;
  bool accept = true;
};
static bool pcm_sink(void* opaque, std::uint32_t port, const PcmBlock& block) noexcept {
  auto& r = *static_cast<Receiver*>(opaque);
  r.last_port = port; r.last_sample = block.sample_index; ++r.pcm_calls;
  for (float x : block.samples) r.sum += x;
  return r.accept;
}
static bool midi_sink(void* opaque, std::uint32_t port, const MidiUmp& msg) noexcept {
  auto& r = *static_cast<Receiver*>(opaque);
  r.last_port = port; r.last_sample = msg.timestamp_ns;
  r.midi_words = msg.word_count; ++r.midi_calls;
  return r.accept;
}

static void test_digital_audio_and_midi_routing() {
  MediaPatchbay<8, 8> m;
  Receiver audio{}, midi{}, monitor{};
  const MediaPort mic{1, MediaKind::pcm, MediaRole::producer, 2, 48000, 7, true};
  const MediaPort asr{2, MediaKind::pcm, MediaRole::consumer, 2, 48000, 7, true};
  const MediaPort out{3, MediaKind::pcm, MediaRole::consumer, 2, 48000, 7, true};
  const MediaPort keys{4, MediaKind::midi_ump, MediaRole::producer, 16, 0, 8, true};
  const MediaPort synth{5, MediaKind::midi_ump, MediaRole::consumer, 16, 0, 8, true};
  CHECK(m.add_port(mic) == MediaError::ok);
  CHECK(m.add_port(asr, pcm_sink, nullptr, &audio) == MediaError::ok);
  CHECK(m.add_port(out, pcm_sink, nullptr, &monitor) == MediaError::ok);
  CHECK(m.add_port(keys) == MediaError::ok);
  CHECK(m.add_port(synth, nullptr, midi_sink, &midi) == MediaError::ok);

  CHECK(m.route(1, 2, m.revision(), false) == MediaError::unauthorized);
  CHECK(m.route(1, 4, m.revision(), true) == MediaError::wrong_direction);
  CHECK(m.route(1, 2, m.revision() - 1, true) == MediaError::bad_revision);
  CHECK(m.route(1, 2, m.revision(), true) == MediaError::ok);
  CHECK(m.route(1, 2, m.revision(), true) == MediaError::duplicate_route);
  CHECK(m.route(1, 3, m.revision(), true) == MediaError::ok);
  CHECK(m.route(4, 5, m.revision(), true) == MediaError::ok);

  const std::array<float, 6> sample{{0.5f, 1.f, -0.5f, 2.f, -1.f, 3.f}};
  const PcmBlock block{sample, 2, 48000, 128};
  auto delivered = m.submit_pcm(1, block);
  CHECK(delivered.status == MediaError::ok);
  CHECK(delivered.attempted == 2 && delivered.delivered == 2 && delivered.rejected == 0);
  CHECK(audio.pcm_calls == 1 && audio.sum == 5.f && audio.last_sample == 128);
  CHECK(monitor.pcm_calls == 1 && monitor.sum == 5.f && monitor.last_port == 3);
  midi.accept = false;
  const MidiUmp event{{0x40903C00u, 0, 0, 0}, 1, 991};
  auto midi_result = m.submit_midi(4, event);
  CHECK(midi_result.attempted == 1 && midi_result.rejected == 1);
  CHECK(midi.midi_calls == 1 && midi.midi_words == 1 && midi.last_sample == 991);
  midi.accept = true;
  midi_result = m.submit_midi(4, event);
  CHECK(midi_result.delivered == 1);
  CHECK(m.submit_midi(4, MidiUmp{}).status == MediaError::invalid_frame);
  CHECK(m.rejected_frames() == 1);

  const auto revision = m.revision();
  CHECK(m.remove_port(2) == MediaError::ok);
  CHECK(m.revision() == revision + 1 && m.muted_routes() == 1);
  delivered = m.submit_pcm(1, block);
  CHECK(delivered.attempted == 1 && delivered.delivered == 1);
  CHECK(audio.pcm_calls == 1); // no call to detached consumer
  CHECK(m.remove_port(3) == MediaError::ok);
  CHECK(m.submit_pcm(1, block).status == MediaError::no_route);
}
static void test_invalid_caps_and_bounded_routes() {
  MediaPatchbay<4, 1> m;
  Receiver r{};
  CHECK(m.add_port(MediaPort{1, MediaKind::pcm, MediaRole::producer, 2, 48000, 1, false}) ==
        MediaError::unverified_port);
  CHECK(m.add_port(MediaPort{1, MediaKind::pcm, MediaRole::producer, 2, 48000, 1, true}) == MediaError::ok);
  CHECK(m.add_port(MediaPort{1, MediaKind::pcm, MediaRole::producer, 2, 48000, 1, true}) == MediaError::duplicate_port);
  CHECK(m.add_port(MediaPort{2, MediaKind::pcm, MediaRole::consumer, 2, 96000, 1, true}, pcm_sink, nullptr, &r) == MediaError::ok);
  CHECK(m.route(1, 2, m.revision(), true) == MediaError::incompatible_format);
  CHECK(m.remove_port(2) == MediaError::ok);
  CHECK(m.add_port(MediaPort{2, MediaKind::pcm, MediaRole::consumer, 2, 48000, 1, true}, pcm_sink, nullptr, &r) == MediaError::ok);
  CHECK(m.add_port(MediaPort{3, MediaKind::pcm, MediaRole::consumer, 2, 48000, 1, true}, pcm_sink, nullptr, &r) == MediaError::ok);
  CHECK(m.route(1, 2, m.revision(), true) == MediaError::ok);
  CHECK(m.route(1, 3, m.revision(), true) == MediaError::routes_full);
  const std::array<float, 3> bad{{1.f, 2.f, 3.f}};
  CHECK(m.submit_pcm(1, PcmBlock{bad, 2, 48000, 0}).status == MediaError::invalid_frame);
  CHECK(m.add_port(MediaPort{4, MediaKind::pcm, MediaRole::consumer, 2, 48000, 1, true}) == MediaError::invalid_port);
}
int main() {
  test_digital_audio_and_midi_routing();
  test_invalid_caps_and_bounded_routes();
  std::cout << "AVM1 virtual PCM/MIDI routing tests passed\n";
  return 0;
}
