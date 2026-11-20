#include "anaiero/sdk/observer.hpp"
#include <chrono>
namespace anaiero::sdk {
std::uint64_t SteadyObserverClock::now_ns()const noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
    std::chrono::steady_clock::now().time_since_epoch()).count());
}
}
