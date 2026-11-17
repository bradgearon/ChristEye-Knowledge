#pragma once
// Recorded simulator features are OFFLINE model outputs, not live observations.
#include "anaiero/contracts/scientific_simulation.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace anaiero::science {
inline constexpr std::size_t kTraceLimit=512;
struct ScientificTrace {
  Provider provider=Provider::native_reference;
  Channel channel=Channel::electrical_activity;
  Evidence evidence=Evidence::computational_prototype;
  std::uint32_t source_epoch=0;
  std::string model_sha256,raw_sha256,transform_sha256,file_sha256;
  std::uint16_t count=0;
  std::array<std::int32_t,kTraceLimit> features{};
};
} // namespace anaiero::science
