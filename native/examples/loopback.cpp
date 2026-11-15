// Digital-only callback loopback: tests notification-to-drain behavior.
#include "christeye/wind/packet.hpp"
#include "christeye/wind/scheduler.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>

using namespace christeye::wind;
struct LocalReceiver {
  BoundedScheduler<4>* queue = nullptr;
  std::uint64_t received_token = 0;
  int callbacks = 0;
};
static void on_ready(void* context) {
  auto& receiver = *static_cast<LocalReceiver*>(context);
  ++receiver.callbacks;
  // Called by a transport/RTOS event notification, not a timed fetch loop.
  while (receiver.queue->pending() != 0) {
    auto delivered = receiver.queue->next(0);
    if (delivered.status == TakeStatus::ready) receiver.received_token = delivered.item.token;
  }
}
int main() {
  Header packet{};
  packet.kind = Kind::observation;
  packet.payload_bytes = 1;
  packet.source_slot = 1;
  packet.destination_slot = 2;
  packet.sequence = 1;
  std::array<std::byte, 1> payload{{std::byte{0x42}}};
  std::array<std::byte, 57> wire{};
  std::size_t written = 0;
  if (encode(packet, {}, payload, wire, written) != Error::ok) return 1;
  View view{};
  if (decode(std::span<const std::byte>(wire.data(), written), view) != Error::ok) return 2;
  LocalReceiver receiver;
  BoundedScheduler<4> scheduler(on_ready, &receiver);
  receiver.queue = &scheduler;
  const auto result = scheduler.post(view.header.delivery_class, {view.header.sequence, 0});
  if (result != PostResult::accepted || receiver.received_token != 1 || receiver.callbacks != 1 ||
      scheduler.pending() != 0) return 3;
  std::cout << "WindBus digital loopback delivered from callback\n";
  return 0;
}
