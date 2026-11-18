#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace anaiero::sdk {
using Bytes = std::string; // Owned binary-safe bytes; no borrowed asynchronous payloads.
using Digest = std::string;
enum class Code : std::uint32_t {
  ok=0, invalid, unsupported, conflict, capacity, not_found, corrupt, io_error,
  committed_degraded, cancelled, expired, unauthorized, history_expired, busy, completed,
  archive_required,archive_unavailable,rebootstrap_required,deadline
};
struct Result {
  Code code=Code::ok;
  std::string message;
  bool ok() const noexcept { return code==Code::ok; }
  bool committed() const noexcept { return code==Code::ok||code==Code::committed_degraded; }
  explicit operator bool() const noexcept { return ok(); }
};
struct Envelope {
  std::string schema;
  Bytes payload;
  bool operator==(const Envelope&) const = default;
};
struct Limits {
  std::size_t max_payload_bytes=4U*1024U*1024U;
  std::size_t max_checkpoint_bytes=4U*1024U*1024U;
  std::size_t max_observations=128;
  std::size_t max_receipts=128;
};
Digest sha256(std::string_view bytes);
bool valid_digest(std::string_view value) noexcept;
bool valid_token(std::string_view value, std::size_t maximum=128) noexcept;
std::string_view code_name(Code code) noexcept;
// Canonical binary length-prefixed record codec; preserves embedded NUL bytes.
Bytes pack(std::span<const std::string> fields);
inline Bytes pack(std::initializer_list<std::string> fields) { return pack(std::span<const std::string>(fields.begin(),fields.size())); }
Result unpack(std::string_view bytes, std::vector<std::string>& fields,
              std::size_t max_fields=256, std::size_t max_bytes=16U*1024U*1024U);
Bytes encode_envelope(const Envelope& value);
Result decode_envelope(std::string_view bytes, Envelope& value);
}
