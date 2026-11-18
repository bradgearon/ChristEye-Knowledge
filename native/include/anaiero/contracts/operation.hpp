#pragma once
// Stable operation identities are scoped to one persisted session.
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
namespace anaiero::operations {
inline constexpr std::size_t kRetainedOperations=64;
enum class State:std::uint8_t {accepted=1,running,completed,failed,cancelled};
enum class Result:std::uint8_t {none=0,ok,invalid,conflict,expired,busy,persistence,
  engine_failure,cancelled,interrupted,capacity};
struct Entry {
  std::uint64_t id=0,session_revision=0,evidence_revision=0;
  State state=State::accepted;
  Result result=Result::none;
  std::string detail;
};
struct View {
  std::uint64_t next_id=1,expired_through=0,active_id=0;
  std::size_t count=0;
  std::array<Entry,kRetainedOperations> entries{};
};
inline constexpr bool terminal(State value)noexcept {
  return value==State::completed||value==State::failed||value==State::cancelled;
}
} // namespace anaiero::operations
