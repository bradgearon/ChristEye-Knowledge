#pragma once
// Immutable engine checkpoint index, separate from simulation physics.
#include <cstdint>
#include <string>
#include <string_view>
namespace anaiero::engine {
struct CheckpointRecord {
  std::uint32_t engine=0,source=0;
  std::uint64_t epoch=0,revision=0;
  std::string model_sha256,checkpoint_sha256;
};
struct ICheckpointCatalog {
  virtual ~ICheckpointCatalog()=default;
  virtual bool commit(const CheckpointRecord&,std::string_view checkpoint) noexcept=0;
  virtual bool latest(std::uint32_t engine,std::uint64_t epoch,
    CheckpointRecord&,std::string& checkpoint) noexcept=0;
};
} // namespace anaiero::engine
