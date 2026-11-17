#pragma once
// Immutable artifact transfer. Gateway authorization grants exact root hashes;
// a digest carried in a request never grants permission by itself.
#include "anaiero/contracts/mesh_identity.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace anaiero::mesh {
inline constexpr std::size_t kMaxMeshArtifactBytes=4*1024*1024;
inline constexpr std::size_t kMeshArtifactChunkBytes=16*1024;
enum class ArtifactCode : std::uint8_t {ok,invalid,denied,not_found,conflict,capacity,corrupt,persistence_failed};
struct ArtifactReceipt {
  std::string sha256;
  std::uint64_t bytes=0;
  bool operator==(const ArtifactReceipt&)const=default;
};
struct ArtifactChunk {
  ArtifactReceipt artifact{};
  std::uint64_t offset=0;
  std::array<std::byte,kMeshArtifactChunkBytes> bytes{};
  std::size_t count=0;
  bool final=false;
};
struct IMeshArtifactStore {
  virtual ~IMeshArtifactStore()=default;
  virtual ArtifactCode describe(std::string_view sha256,std::uint64_t& bytes)noexcept=0;
  virtual ArtifactCode read(std::string_view sha256,std::uint64_t offset,std::span<std::byte>,std::size_t& count)noexcept=0;
  virtual ArtifactCode completed(const ArtifactReceipt&,bool& found)noexcept=0;
  // Full digest and size are checked before the durable completion receipt.
  virtual ArtifactCode commit(const ArtifactReceipt&,std::string_view bytes)noexcept=0;
};
} // namespace anaiero::mesh
