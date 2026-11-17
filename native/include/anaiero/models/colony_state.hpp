#pragma once
// CE-SW-18: plain bounded ColonyState data. No behavior or equipment control.
#include <array>
#include <cstddef>
#include <cstdint>
namespace anaiero::runtime {
enum class Metric : std::uint8_t {
  viability=1, energy, support, stress, activity, digital_learning,
  digital_desire, connection, acquisition_health, digital_route_active, membrane_voltage
};
enum class Origin : std::uint8_t { simulated=1, external_unverified_readonly=2 };
enum class Quality : std::uint8_t { unknown=0, current=1, degraded=2, stale=3 };
enum class Lane : std::uint8_t { fault=0, health=1, desire=2, evidence=3, telemetry=4 };
inline constexpr std::size_t kMaxMetrics=256;
inline constexpr std::size_t kMaxSources=32;
inline constexpr std::size_t kMaxWork=96;
struct MetricValue {
  std::uint32_t colony=0, animon=0, source=0;
  Metric metric=Metric::energy;
  std::int32_t value=0;
  Origin origin=Origin::simulated;
  Quality quality=Quality::unknown;
  std::uint64_t sequence=0, acquired_ns=0, evidence_tag=0;
  std::uint64_t source_epoch=0; // zero preserves legacy WindPacket/1 source behavior
};
struct SourceCursor {
  std::uint32_t id=0, clock_domain=0;
  std::uint64_t last_sequence=0, last_acquired_ns=0;
  bool has_sequence=false, gap=false;
  std::uint64_t epoch=0; // sequence scope: (source, epoch)
};
struct ColonyState {
  std::uint64_t revision=0, committed_updates=0, sequence_gaps=0;
  std::uint16_t count=0, sources=0;
  std::array<MetricValue,kMaxMetrics> values{};
  std::array<SourceCursor,kMaxSources> cursors{};
};
}  // namespace anaiero::runtime
