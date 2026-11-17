#pragma once
// Authenticated transport evidence is produced by the native TLS adapter.
// Possessing a valid certificate grants no application publication or read scope.
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
namespace anaiero::mesh {
inline constexpr std::size_t kMaxTransportFrameBytes=65536;
enum class TransportCode : std::uint8_t {
  ok,invalid,permission_denied,bind_failed,connection_failed,timeout,cancelled,
  closed,io_error,tls_error,unverified_peer,protocol_error,frame_too_large
};
struct Endpoint {std::string address="127.0.0.1";std::uint16_t port=0;};
struct IAuthenticatedPeer {
  virtual ~IAuthenticatedPeer()=default;
  virtual bool verified()const noexcept=0;
  virtual std::string_view certificate_sha256()const noexcept=0;
  virtual std::uint64_t certificate_expiry_unix_seconds()const noexcept=0;
};
inline const char* transport_code_name(TransportCode code) noexcept {
  switch(code){
    case TransportCode::ok:return "ok";
    case TransportCode::invalid:return "invalid";
    case TransportCode::permission_denied:return "permission_denied";
    case TransportCode::bind_failed:return "bind_failed";
    case TransportCode::connection_failed:return "connection_failed";
    case TransportCode::timeout:return "timeout";
    case TransportCode::cancelled:return "cancelled";
    case TransportCode::closed:return "closed";
    case TransportCode::io_error:return "io_error";
    case TransportCode::tls_error:return "tls_error";
    case TransportCode::unverified_peer:return "unverified_peer";
    case TransportCode::protocol_error:return "protocol_error";
    case TransportCode::frame_too_large:return "frame_too_large";
  }
  return "invalid";
}
} // namespace anaiero::mesh
