#pragma once
// All gateway calls are serialized on one host owner, including administration.
// Session handles are nonzero and remain valid until close_peer is delivered.
#include "anaiero/contracts/mesh_transport.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
namespace anaiero::mesh {
inline constexpr std::size_t kMeshHostQueueCapacity=16;
inline constexpr std::size_t kMeshHostClientCapacity=8;
struct IMeshGateway {
  virtual ~IMeshGateway()=default;
  virtual bool open_peer(const IAuthenticatedPeer&,std::uint64_t& session)=0;
  virtual std::string request(std::uint64_t session,std::string_view request)=0;
  virtual void close_peer(std::uint64_t session)=0;
  virtual std::string administer(std::string_view request)=0;
};
} // namespace anaiero::mesh
