#pragma once
// SIM1 instruction set: 32-byte little-endian bytecode for a DIGITAL surrogate.
// This is NOT a neuronal signal encoder, signed program, or physical actuator.
#include "christeye/living/simulator.hpp"
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>

namespace christeye::living {
inline constexpr std::size_t kInstructionBytes = 32;
enum class BytecodeError : std::uint8_t {
  ok, bad_size, bad_magic, unsupported_version, reserved_nonzero, invalid_opcode
};
namespace detail {
inline void put32(std::span<std::byte> b, std::size_t offset,
                  std::uint32_t v) noexcept {
  for (unsigned i = 0; i < 4; ++i) {
    b[offset + i] = std::byte(v & 0xffu);
    v >>= 8;
  }
}
inline std::uint32_t get32(std::span<const std::byte> b,
                            std::size_t offset) noexcept {
  std::uint32_t v = 0;
  for (unsigned i = 0; i < 4; ++i)
    v |= std::uint32_t(std::to_integer<std::uint8_t>(b[offset + i])) << (8 * i);
  return v;
}
}
inline BytecodeError encode_instruction(const Instruction& instruction,
                                        std::span<std::byte> frame) noexcept {
  if (frame.size() != kInstructionBytes) return BytecodeError::bad_size;
  const auto opcode = static_cast<std::uint8_t>(instruction.op);
  if (opcode < 1 || opcode > static_cast<std::uint8_t>(Op::observe))
    return BytecodeError::invalid_opcode;
  frame[0] = std::byte{'C'}; frame[1] = std::byte{'E'};
  frame[2] = std::byte{'I'}; frame[3] = std::byte{'P'};
  frame[4] = std::byte{1}; // fixed bytecode version
  frame[5] = std::byte{opcode};
  frame[6] = std::byte{0}; frame[7] = std::byte{0};
  detail::put32(frame, 8, instruction.subject);
  detail::put32(frame, 12, instruction.target);
  detail::put32(frame, 16, std::bit_cast<std::uint32_t>(instruction.value));
  detail::put32(frame, 20, std::bit_cast<std::uint32_t>(instruction.extra));
  detail::put32(frame, 24, instruction.parent);
  detail::put32(frame, 28, 0);
  return BytecodeError::ok;
}
inline BytecodeError decode_instruction(std::span<const std::byte> frame,
                                        Instruction& out) noexcept {
  out = Instruction{};
  if (frame.size() != kInstructionBytes) return BytecodeError::bad_size;
  if (frame[0] != std::byte{'C'} || frame[1] != std::byte{'E'} ||
      frame[2] != std::byte{'I'} || frame[3] != std::byte{'P'})
    return BytecodeError::bad_magic;
  if (frame[4] != std::byte{1}) return BytecodeError::unsupported_version;
  if (frame[6] != std::byte{0} || frame[7] != std::byte{0} ||
      detail::get32(frame, 28) != 0) return BytecodeError::reserved_nonzero;
  const auto opcode = std::to_integer<std::uint8_t>(frame[5]);
  if (opcode < 1 || opcode > static_cast<std::uint8_t>(Op::observe))
    return BytecodeError::invalid_opcode;
  out = {static_cast<Op>(opcode), detail::get32(frame, 8), detail::get32(frame, 12),
         std::bit_cast<std::int32_t>(detail::get32(frame, 16)),
         std::bit_cast<std::int32_t>(detail::get32(frame, 20)),
         detail::get32(frame, 24)};
  return BytecodeError::ok;
}

struct ProgramOutcome {
  BytecodeError wire_error = BytecodeError::ok;
  Status simulation_status = Status::ok;
  std::size_t processed = 0;
};
// Transaction semantics are PER INSTRUCTION; earlier successfully applied
// commands remain on later failure. Caller can take a bounded full checkpoint
// and restore it if all-or-nothing program semantics are required.
template<class Sim>
ProgramOutcome run_program(Sim& sim, std::span<const std::byte> stream) noexcept {
  if (stream.size() % kInstructionBytes != 0)
    return {BytecodeError::bad_size, Status::ok, 0};
  std::size_t index = 0;
  for (std::size_t offset = 0; offset < stream.size(); offset += kInstructionBytes) {
    Instruction instruction{};
    const BytecodeError wire = decode_instruction(stream.subspan(offset, kInstructionBytes), instruction);
    if (wire != BytecodeError::ok) return {wire, Status::ok, index};
    const Status result = sim.apply(instruction);
    if (result != Status::ok) return {BytecodeError::ok, result, index};
    ++index;
  }
  return {BytecodeError::ok, Status::ok, index};
}
} // namespace christeye::living
