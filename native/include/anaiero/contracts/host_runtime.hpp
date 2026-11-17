#pragma once
// Portable local host lifecycle and request/response. No remote mesh,
// physical instrument, care or stimulation command is expressible here.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
namespace anaiero::host {
enum class Code : std::uint8_t {ok,invalid,bind_failure,permission_denied,
                                 read_failure,write_failure,interrupted};
struct Reply {
  std::array<char,256> bytes{};
  std::size_t length=0;
  bool stop=false;
};
struct ICommandService {
  virtual ~ICommandService()=default;
  virtual Reply handle(std::string_view request) noexcept=0;
};
struct ILocalServer {
  virtual ~ILocalServer()=default;
  virtual Code serve(ICommandService&,std::size_t max_requests=0) noexcept=0;
};
} // namespace anaiero::host
