#pragma once
// CE-SW-01 WindPacket/1: allocation-free binary envelope codec.
// The wire ABI is independent of C++ structure padding and machine endianness.
// IMPORTANT: This codec is NOT a secure channel, signature verifier, ACL, or actuator.
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

namespace christeye::wind {
inline constexpr std::size_t kFixedHeaderBytes = 56;
inline constexpr std::uint8_t kMajorVersion = 1;
inline constexpr std::uint8_t kMinorVersion = 0;

// Flags are advisory wire metadata; authentication is enforced by an external
// transport/session implementation and NEVER inferred from a header flag.
inline constexpr std::uint16_t kKnownFlags = 0x001Fu;

enum class Error : std::uint8_t {
  ok, invalid_argument, bad_magic, unsupported_version, unsupported_flags,
  invalid_kind, invalid_class, invalid_length, exceeds_limits,
  truncated, unknown_required_tlv, duplicate_singleton_tlv
};

enum class Kind : std::uint8_t {
  observation = 1, evidence = 2, change = 3, receipt = 4,
  capability = 5, control = 6, status = 7, object = 8, sync = 9
};

enum class DeliveryClass : std::uint8_t {
  fault = 0, decision = 1, acquisition = 2, evidence = 3, status = 4
};

struct Limits {
  std::size_t max_header_bytes = 256;
  std::size_t max_payload_bytes = 64 * 1024;
  std::size_t max_frame_bytes = 64 * 1024 + 256;
};

struct Header {
  std::uint8_t major = kMajorVersion;
  std::uint8_t minor = kMinorVersion;
  std::uint16_t flags = 0;
  std::uint16_t header_bytes = kFixedHeaderBytes;
  Kind kind = Kind::observation;
  DeliveryClass delivery_class = DeliveryClass::acquisition;
  std::uint32_t source_slot = 0;
  std::uint32_t destination_slot = 0;
  std::uint32_t stream_id = 0;
  std::uint64_t sequence = 0;
  std::array<std::byte, 16> message_id{};
  std::uint32_t payload_bytes = 0;
  std::uint32_t schema_id = 0;
};

struct View {
  Header header{};
  std::span<const std::byte> extensions{};
  std::span<const std::byte> payload{};
};

namespace detail {
inline std::uint16_t get16(std::span<const std::byte> b, std::size_t at) noexcept {
  return std::uint16_t(std::uint32_t(std::to_integer<std::uint8_t>(b[at])) |
         (std::uint32_t(std::to_integer<std::uint8_t>(b[at + 1])) << 8));
}
inline std::uint32_t get32(std::span<const std::byte> b, std::size_t at) noexcept {
  return std::uint32_t(get16(b, at)) | (std::uint32_t(get16(b, at + 2)) << 16);
}
inline std::uint64_t get64(std::span<const std::byte> b, std::size_t at) noexcept {
  return std::uint64_t(get32(b, at)) | (std::uint64_t(get32(b, at + 4)) << 32);
}
inline void put16(std::span<std::byte> b, std::size_t at, std::uint16_t v) noexcept {
  b[at] = std::byte(v & 0xff);
  b[at+1] = std::byte((v >> 8) & 0xff);
}
inline void put32(std::span<std::byte> b, std::size_t at, std::uint32_t v) noexcept {
  put16(b, at, std::uint16_t(v));
  put16(b, at + 2, std::uint16_t(v >> 16));
}
inline void put64(std::span<std::byte> b, std::size_t at, std::uint64_t v) noexcept {
  put32(b, at, std::uint32_t(v));
  put32(b, at + 4, std::uint32_t(v >> 32));
}
inline bool valid_kind(Kind k) noexcept {
  return std::uint8_t(k) >= 1 && std::uint8_t(k) <= 9;
}
inline bool valid_class(DeliveryClass k) noexcept { return std::uint8_t(k) <= 4; }

// Bit 15 means REQUIRED, bits 0..14 identify the TLV.
// Known v1 IDs are 1..16. Parents (6) is a permitted repeatable TLV;
// all remaining known IDs are singletons in the initial profile.
inline Error validate_tlvs(std::span<const std::byte> tlvs) noexcept {
  std::uint32_t seen = 0;
  std::size_t pos = 0;
  while (pos < tlvs.size()) {
    if (tlvs.size() - pos < 4) return Error::truncated;
    const auto raw_type = get16(tlvs, pos);
    const auto len = get16(tlvs, pos + 2);
    pos += 4;
    if (len > tlvs.size() - pos) return Error::truncated;
    const std::uint16_t id = raw_type & 0x7fff;
    if (id == 0) return Error::invalid_argument;
    if (id > 16) {
      if ((raw_type & 0x8000u) != 0) return Error::unknown_required_tlv;
    } else if (id != 6) {
      const std::uint32_t bit = std::uint32_t{1} << (id - 1);
      if ((seen & bit) != 0) return Error::duplicate_singleton_tlv;
      seen |= bit;
    }
    pos += len;
  }
  return Error::ok;
}
inline Error validate_header(const Header& h, std::size_t tlvs,
                             std::size_t payload, Limits limits) noexcept {
  if (h.major != kMajorVersion || h.minor > kMinorVersion) return Error::unsupported_version;
  if ((h.flags & ~kKnownFlags) != 0) return Error::unsupported_flags;
  if (!valid_kind(h.kind)) return Error::invalid_kind;
  if (!valid_class(h.delivery_class)) return Error::invalid_class;
  if (tlvs > std::numeric_limits<std::uint16_t>::max() - kFixedHeaderBytes ||
      payload > std::numeric_limits<std::uint32_t>::max()) return Error::exceeds_limits;
  if (h.header_bytes != kFixedHeaderBytes + tlvs || h.payload_bytes != payload)
    return Error::invalid_length;
  if (h.header_bytes > limits.max_header_bytes || payload > limits.max_payload_bytes ||
      (kFixedHeaderBytes + tlvs > limits.max_frame_bytes) ||
      payload > limits.max_frame_bytes - (kFixedHeaderBytes + tlvs))
    return Error::exceeds_limits;
  return Error::ok;
}
}  // namespace detail

// On success exactly header_bytes + payload_bytes are written; output buffers
// larger than the frame are valid. On error, written remains zero.
inline Error encode(const Header& h, std::span<const std::byte> extensions,
                    std::span<const std::byte> payload, std::span<std::byte> output,
                    std::size_t& written, Limits limits = {}) noexcept {
  written = 0;
  auto e = detail::validate_header(h, extensions.size(), payload.size(), limits);
  if (e != Error::ok) return e;
  if ((e = detail::validate_tlvs(extensions)) != Error::ok) return e;
  const std::size_t frame_size = std::size_t(h.header_bytes) + payload.size();
  if (output.size() < frame_size) return Error::truncated;
  output[0] = std::byte{'C'}; output[1] = std::byte{'E'};
  output[2] = std::byte{'W'}; output[3] = std::byte{'P'};
  output[4] = std::byte(h.major); output[5] = std::byte(h.minor);
  detail::put16(output, 6, h.flags);
  detail::put16(output, 8, h.header_bytes);
  output[10] = std::byte(std::uint8_t(h.kind));
  output[11] = std::byte(std::uint8_t(h.delivery_class));
  detail::put32(output, 12, h.source_slot);
  detail::put32(output, 16, h.destination_slot);
  detail::put32(output, 20, h.stream_id);
  detail::put64(output, 24, h.sequence);
  for (std::size_t i = 0; i < h.message_id.size(); ++i) output[32 + i] = h.message_id[i];
  detail::put32(output, 48, h.payload_bytes);
  detail::put32(output, 52, h.schema_id);
  for (std::size_t i = 0; i < extensions.size(); ++i) output[kFixedHeaderBytes + i] = extensions[i];
  for (std::size_t i = 0; i < payload.size(); ++i) output[h.header_bytes + i] = payload[i];
  written = frame_size;
  return Error::ok;
}

// decode requires exactly one complete authenticated-application-frame worth
// of bytes (transport segmentation is handled before this function).
// Returned spans borrow frame memory; caller must preserve its lifetime.
inline Error decode(std::span<const std::byte> frame, View& view,
                    Limits limits = {}) noexcept {
  view = View{};
  if (frame.size() < kFixedHeaderBytes) return Error::truncated;
  if (frame[0] != std::byte{'C'} || frame[1] != std::byte{'E'} ||
      frame[2] != std::byte{'W'} || frame[3] != std::byte{'P'}) return Error::bad_magic;
  Header h{};
  h.major = std::to_integer<std::uint8_t>(frame[4]);
  h.minor = std::to_integer<std::uint8_t>(frame[5]);
  h.flags = detail::get16(frame, 6);
  h.header_bytes = detail::get16(frame, 8);
  h.kind = Kind(std::to_integer<std::uint8_t>(frame[10]));
  h.delivery_class = DeliveryClass(std::to_integer<std::uint8_t>(frame[11]));
  h.source_slot = detail::get32(frame, 12);
  h.destination_slot = detail::get32(frame, 16);
  h.stream_id = detail::get32(frame, 20);
  h.sequence = detail::get64(frame, 24);
  for (std::size_t i = 0; i < h.message_id.size(); ++i) h.message_id[i] = frame[32 + i];
  h.payload_bytes = detail::get32(frame, 48);
  h.schema_id = detail::get32(frame, 52);
  if (h.header_bytes < kFixedHeaderBytes) return Error::invalid_length;
  const std::size_t ext_bytes = h.header_bytes - kFixedHeaderBytes;
  const auto e = detail::validate_header(h, ext_bytes, h.payload_bytes, limits);
  if (e != Error::ok) return e;
  const std::size_t expected = std::size_t(h.header_bytes) + h.payload_bytes;
  if (expected > frame.size()) return Error::truncated;
  if (expected != frame.size()) return Error::invalid_length;
  const auto extensions = frame.subspan(kFixedHeaderBytes, ext_bytes);
  if (const auto x = detail::validate_tlvs(extensions); x != Error::ok) return x;
  view.header = h;
  view.extensions = extensions;
  view.payload = frame.subspan(h.header_bytes, h.payload_bytes);
  return Error::ok;
}
}  // namespace christeye::wind
