#include "christeye/wind/packet.hpp"
#include "christeye/wind/scheduler.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>

using namespace christeye::wind;
#define CHECK(expr) do { if (!(expr)) { std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << " " #expr "\n"; std::exit(1); } } while (false)

static void packet_roundtrip() {
  Header h{};
  h.flags = 1;
  h.header_bytes = 64;
  h.kind = Kind::evidence;
  h.delivery_class = DeliveryClass::evidence;
  h.source_slot = 0x11223344;
  h.destination_slot = 42;
  h.stream_id = 5;
  h.sequence = 0x0102030405060708ULL;
  h.message_id[0] = std::byte{0xAB};
  h.payload_bytes = 3;
  h.schema_id = 0xAABBCCDD;
  std::array<std::byte, 8> tlvs{{std::byte{0x01}, std::byte{0x00}, std::byte{0x04}, std::byte{0x00},
                                  std::byte{0x01}, std::byte{0x02}, std::byte{0x03}, std::byte{0x04}}};
  std::array<std::byte, 3> payload{{std::byte{0xAA}, std::byte{0xBB}, std::byte{0xCC}}};
  std::array<std::byte, 128> frame{};
  std::size_t written = 12;
  CHECK(encode(h, tlvs, payload, frame, written) == Error::ok);
  CHECK(written == 67);
  CHECK(frame[0] == std::byte{'C'} && frame[3] == std::byte{'P'});
  CHECK(frame[12] == std::byte{0x44} && frame[15] == std::byte{0x11});
  CHECK(frame[24] == std::byte{0x08} && frame[31] == std::byte{0x01});
  CHECK(frame[52] == std::byte{0xDD} && frame[55] == std::byte{0xAA});
  View result{};
  CHECK(decode(std::span<const std::byte>(frame.data(), written), result) == Error::ok);
  CHECK(result.header.sequence == h.sequence);
  CHECK(result.header.schema_id == h.schema_id);
  CHECK(result.header.message_id == h.message_id);
  CHECK(result.payload.size() == 3 && result.payload[2] == std::byte{0xCC});
  CHECK(result.extensions.size() == 8);
  frame[0] = std::byte{0};
  CHECK(decode(std::span<const std::byte>(frame.data(), written), result) == Error::bad_magic);
  frame[0] = std::byte{'C'};
  CHECK(decode(std::span<const std::byte>(frame.data(), written - 1), result) == Error::truncated);
  CHECK(decode(std::span<const std::byte>(frame.data(), written + 1), result) == Error::invalid_length);
  frame[4] = std::byte{2};
  CHECK(decode(std::span<const std::byte>(frame.data(), written), result) == Error::unsupported_version);
  frame[4] = std::byte{1};
  frame[48] = std::byte{0xff}; frame[49] = std::byte{0xff}; frame[50] = std::byte{0x01};
  CHECK(decode(std::span<const std::byte>(frame.data(), written), result) == Error::exceeds_limits);
}

static void malformed_tlvs() {
  Header h{}; h.header_bytes = 60;
  std::array<std::byte, 4> ext{{std::byte{0xff}, std::byte{0x80}, std::byte{0}, std::byte{0}}};
  std::array<std::byte, 64> frame{}; std::size_t written = 0;
  CHECK(encode(h, ext, {}, frame, written) == Error::unknown_required_tlv);
  ext[1] = std::byte{0};
  CHECK(encode(h, ext, {}, frame, written) == Error::ok); // optional unknown is ignored
  h.header_bytes = 64;
  std::array<std::byte, 8> dup{{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0},
                                 std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}}};
  CHECK(encode(h, dup, {}, frame, written) == Error::duplicate_singleton_tlv);
  h.header_bytes = 60;
  ext = {{std::byte{1}, std::byte{0}, std::byte{9}, std::byte{0}}};
  CHECK(encode(h, ext, {}, frame, written) == Error::truncated);
  h.header_bytes = 56;
  CHECK(encode(h, {}, {}, frame, written, Limits{56, 0, 56}) == Error::ok);
  CHECK(encode(h, {}, {}, frame, written, Limits{55, 0, 56}) == Error::exceeds_limits);
}

static void random_input_never_crashes() {
  std::minstd_rand rng(12345u);
  std::array<std::byte, 200> data{}; View result{};
  for (int i = 0; i < 30000; ++i) {
    const std::size_t size = std::size_t(rng() % 200u);
    for (std::size_t j = 0; j < size; ++j) data[j] = std::byte(rng() & 0xffu);
    const Error e = decode(std::span<const std::byte>(data.data(), size), result);
    CHECK(e != Error::ok || (result.payload.data() != nullptr && result.header.header_bytes >= 56));
  }
}

static void notify(void* ptr) { ++*static_cast<int*>(ptr); }
static void scheduler_queues() {
  int calls = 0;
  BoundedScheduler<2> q(notify, &calls);
  CHECK(q.pending() == 0 && q.capacity() == 10);
  CHECK(q.post(DeliveryClass::status, {1, 0}) == PostResult::accepted);
  CHECK(calls == 1);
  CHECK(q.post(DeliveryClass::status, {2, 5}) == PostResult::accepted);
  CHECK(calls == 1);
  CHECK(q.post(DeliveryClass::status, {3, 0}) == PostResult::full);
  CHECK(q.post(DeliveryClass::fault, {4, 0}) == PostResult::accepted);
  CHECK(q.pending(DeliveryClass::fault) == 1);
  CHECK(q.post(DeliveryClass(255), {99, 0}) == PostResult::invalid_class);
  bool observed_expired = false, observed_fault = false;
  while (q.pending()) {
    const auto r = q.next(5);
    CHECK(r.status != TakeStatus::empty);
    if (r.item.token == 2) observed_expired = r.status == TakeStatus::expired;
    if (r.item.token == 4) observed_fault = r.status == TakeStatus::ready;
  }
  CHECK(observed_expired && observed_fault);
  CHECK(q.next(5).status == TakeStatus::empty);
  CHECK(q.post(DeliveryClass::decision, {8, 0}) == PostResult::accepted);
  CHECK(calls == 2); // notification only on empty -> nonempty
}

static void packet_overlapping_buffers_rejected() {
  Header h{};
  h.payload_bytes = 4;
  std::array<std::byte, 64> buffer{};
  buffer[10] = std::byte{0x17};
  std::size_t written = 7;
  const auto input = std::span<const std::byte>(buffer.data() + 10, 4);
  const auto output = std::span<std::byte>(buffer.data(), 64);
  CHECK(encode(h, {}, input, output, written) == Error::overlapping_buffers);
  CHECK(written == 0 && buffer[10] == std::byte{0x17});
  std::array<std::byte, 4> separate{{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}}};
  CHECK(encode(h, {}, separate, output, written) == Error::ok);
  CHECK(written == 60);
  h.header_bytes = 60; h.payload_bytes = 0;
  // A TLV borrowed from the output buffer must also be rejected.
  const auto extension = std::span<const std::byte>(buffer.data() + 15, 4);
  CHECK(encode(h, extension, {}, output, written) == Error::overlapping_buffers);
  CHECK(written == 0);
}

static void scheduler_fairness() {
  BoundedScheduler<128> q;
  for (std::uint64_t i = 0; i < 100; ++i) {
    CHECK(q.post(DeliveryClass::decision, {i, 0}) == PostResult::accepted);
    CHECK(q.post(DeliveryClass::status, {100+i, 0}) == PostResult::accepted);
  }
  int status_seen = 0;
  for (int i = 0; i < 70; ++i) {
    auto r = q.next(1);
    CHECK(r.status == TakeStatus::ready);
    if (r.delivery_class == DeliveryClass::status) ++status_seen;
  }
  CHECK(status_seen == 10); // Interleaved 6:1 share across active lanes.
  CHECK(q.post(DeliveryClass::fault, {999, 0}) == PostResult::accepted);
  CHECK(q.next(1).delivery_class == DeliveryClass::fault);
  CHECK(q.post(DeliveryClass::fault, {1000, 1}) == PostResult::accepted);
  auto expired = q.next(1);
  CHECK(expired.delivery_class == DeliveryClass::fault);
  CHECK(expired.status == TakeStatus::expired);
}

int main() {
  packet_roundtrip(); malformed_tlvs(); random_input_never_crashes();
  packet_overlapping_buffers_rejected();
  scheduler_queues(); scheduler_fairness();
  std::cout << "WindPacket/1 codec and bounded endpoint scheduling tests passed\n";
}
