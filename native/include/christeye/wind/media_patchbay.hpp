#pragma once
// Anaiero AVM1 — deterministic, single-owner, DIGITAL-ONLY media patchbay.
// This module performs no device I/O, cryptography, DSP, biological actuation,
// or asynchronous storage. Port verification must be supplied by a trusted
// enrolment/capability service; caller emits WindBus receipts.
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace christeye::wind {
enum class MediaKind : std::uint8_t { pcm = 1, midi_ump = 2 };
enum class MediaRole : std::uint8_t { producer = 1, consumer = 2 };
enum class MediaError : std::uint8_t {
  ok, invalid_port, duplicate_port, ports_full, unverified_port,
  bad_revision, unauthorized, wrong_direction, incompatible_format,
  duplicate_route, routes_full, no_route, invalid_frame
};
struct MediaPort {
  std::uint32_t id = 0;
  MediaKind kind = MediaKind::pcm;
  MediaRole role = MediaRole::producer;
  std::uint16_t channels = 0;
  std::uint32_t sample_rate_hz = 0;
  std::uint32_t clock_domain = 0;
  bool verified = false;
};
struct PcmBlock {
  // Interleaved float PCM. Valid only for the duration of the callback.
  std::span<const float> samples{};
  std::uint16_t channels = 0;
  std::uint32_t sample_rate_hz = 0;
  std::uint64_t sample_index = 0;
};
struct MidiUmp {
  std::array<std::uint32_t, 4> words{};
  std::uint8_t word_count = 0;
  std::uint64_t timestamp_ns = 0;
};
using PcmReceiver = bool (*)(void*, std::uint32_t, const PcmBlock&) noexcept;
using MidiReceiver = bool (*)(void*, std::uint32_t, const MidiUmp&) noexcept;
struct MediaDispatch {
  MediaError status = MediaError::ok;
  std::uint16_t attempted = 0;
  std::uint16_t delivered = 0;
  std::uint16_t rejected = 0;
};
struct MediaRoute {
  std::uint32_t source = 0;
  std::uint32_t target = 0;
  bool active = false;
};

// Explicit bounds: no allocation, implicit threads, queues or blocking I/O.
// All invocation/route mutations must occur on one serialized media owner.
template<std::size_t MaxPorts, std::size_t MaxRoutes>
class MediaPatchbay {
  static_assert(MaxPorts > 0 && MaxRoutes > 0);
  struct Endpoint {
    MediaPort port{};
    PcmReceiver pcm = nullptr;
    MidiReceiver midi = nullptr;
    void* context = nullptr;
    bool occupied = false;
  };
  std::array<Endpoint, MaxPorts> ports_{};
  std::array<MediaRoute, MaxRoutes> routes_{};
  std::uint64_t revision_ = 0;
  std::uint64_t muted_routes_ = 0;
  std::uint64_t rejected_frames_ = 0;

  Endpoint* lookup(std::uint32_t id) noexcept {
    for (auto& e : ports_)
      if (e.occupied && e.port.id == id) return &e;
    return nullptr;
  }
  const Endpoint* lookup(std::uint32_t id) const noexcept {
    for (const auto& e : ports_)
      if (e.occupied && e.port.id == id) return &e;
    return nullptr;
  }
  static bool compatible(const MediaPort& src, const MediaPort& dst) noexcept {
    if (src.kind != dst.kind || src.clock_domain != dst.clock_domain) return false;
    if (src.role != MediaRole::producer || dst.role != MediaRole::consumer) return false;
    if (src.kind == MediaKind::pcm)
      return src.channels == dst.channels &&
             src.channels > 0 && src.sample_rate_hz > 0 &&
             src.sample_rate_hz == dst.sample_rate_hz;
    return src.kind == MediaKind::midi_ump;
  }

public:
  // A fully verified port is still subject to route-level authorization.
  MediaError add_port(MediaPort port, PcmReceiver pcm = nullptr,
                      MidiReceiver midi = nullptr, void* context = nullptr) noexcept {
    if (port.id == 0 || port.channels == 0 || port.clock_domain == 0 ||
        (port.kind != MediaKind::pcm && port.kind != MediaKind::midi_ump) ||
        (port.role != MediaRole::producer && port.role != MediaRole::consumer))
      return MediaError::invalid_port;
    if (port.kind == MediaKind::pcm && port.sample_rate_hz == 0)
      return MediaError::invalid_port;
    if (!port.verified) return MediaError::unverified_port;
    if (lookup(port.id)) return MediaError::duplicate_port;
    if (port.role == MediaRole::consumer &&
        ((port.kind == MediaKind::pcm && !pcm) ||
         (port.kind == MediaKind::midi_ump && !midi)))
      return MediaError::invalid_port;
    for (auto& e : ports_) {
      if (!e.occupied) {
        e = {port, pcm, midi, context, true};
        ++revision_;
        return MediaError::ok;
      }
    }
    return MediaError::ports_full;
  }

  MediaError route(std::uint32_t source, std::uint32_t target,
                   std::uint64_t expected_revision, bool authorized) noexcept {
    if (expected_revision != revision_) return MediaError::bad_revision;
    if (!authorized) return MediaError::unauthorized;
    const auto* s = lookup(source);
    const auto* d = lookup(target);
    if (!s || !d) return MediaError::invalid_port;
    if (!s->port.verified || !d->port.verified) return MediaError::unverified_port;
    if (s->port.role != MediaRole::producer || d->port.role != MediaRole::consumer)
      return MediaError::wrong_direction;
    if (!compatible(s->port, d->port)) return MediaError::incompatible_format;
    for (const auto& r : routes_)
      if (r.active && r.source == source && r.target == target)
        return MediaError::duplicate_route;
    for (auto& r : routes_) {
      if (!r.active) {
        r = {source, target, true};
        ++revision_;
        return MediaError::ok;
      }
    }
    return MediaError::routes_full;
  }

  // Hot removal deactivates all routes before the port is released. Caller
  // must mute real hardware outside this software-only reference.
  MediaError remove_port(std::uint32_t id) noexcept {
    auto* e = lookup(id);
    if (!e) return MediaError::invalid_port;
    for (auto& r : routes_) {
      if (r.active && (r.source == id || r.target == id)) {
        r.active = false;
        ++muted_routes_;
      }
    }
    e->occupied = false;
    ++revision_;
    return MediaError::ok;
  }

  MediaDispatch submit_pcm(std::uint32_t source, const PcmBlock& block) noexcept {
    const auto* s = lookup(source);
    if (!s || s->port.kind != MediaKind::pcm ||
        s->port.role != MediaRole::producer)
      return {MediaError::invalid_port};
    if (block.channels != s->port.channels ||
        block.sample_rate_hz != s->port.sample_rate_hz ||
        block.samples.empty() ||
        block.samples.size() % block.channels != 0 ||
        block.samples.size() / block.channels > 1024) {
      ++rejected_frames_;
      return {MediaError::invalid_frame};
    }
    MediaDispatch result{};
    for (const auto& route : routes_) {
      if (!route.active || route.source != source) continue;
      auto* target = lookup(route.target);
      if (!target || !target->pcm) continue;
      ++result.attempted;
      if (target->pcm(target->context, target->port.id, block))
        ++result.delivered;
      else
        ++result.rejected;
    }
    if (result.attempted == 0) result.status = MediaError::no_route;
    return result;
  }

  MediaDispatch submit_midi(std::uint32_t source, const MidiUmp& message) noexcept {
    const auto* s = lookup(source);
    if (!s || s->port.kind != MediaKind::midi_ump ||
        s->port.role != MediaRole::producer)
      return {MediaError::invalid_port};
    if (message.word_count == 0 || message.word_count > 4) {
      ++rejected_frames_;
      return {MediaError::invalid_frame};
    }
    MediaDispatch result{};
    for (const auto& route : routes_) {
      if (!route.active || route.source != source) continue;
      auto* target = lookup(route.target);
      if (!target || !target->midi) continue;
      ++result.attempted;
      if (target->midi(target->context, target->port.id, message))
        ++result.delivered;
      else
        ++result.rejected;
    }
    if (result.attempted == 0) result.status = MediaError::no_route;
    return result;
  }

  std::uint64_t revision() const noexcept { return revision_; }
  std::uint64_t muted_routes() const noexcept { return muted_routes_; }
  std::uint64_t rejected_frames() const noexcept { return rejected_frames_; }
  bool port_registered(std::uint32_t id) const noexcept { return lookup(id) != nullptr; }
};
} // namespace christeye::wind
