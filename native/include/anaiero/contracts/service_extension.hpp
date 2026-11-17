#pragma once
// Bounded service extension ABI for trusted, compiled, nonblocking observers.
#include <cstdint>
#include <string>
namespace anaiero::contracts {
enum class ServiceId : std::uint8_t { colony_projector=1, runtime_dispatch=2,
  cell_behavior=3, adaptive_optimization=4, learning=5, movement=6, evidence=7,
  simulation_coordinator=8, operation=9, experiment=10, scientific_solver=11, session=12 };
enum class ServicePhase : std::uint8_t { proposed=1,committed=2,rejected=3,reviewed=4,reverted=5 };
struct ServiceEvent { ServiceId service=ServiceId::adaptive_optimization;
  ServicePhase phase=ServicePhase::proposed;std::uint64_t revision=0;
  std::uint32_t subject=0;std::int32_t status=0;
  std::uint64_t operation_id=0,source_epoch=0;
  std::uint32_t source=0; };
struct IServiceExtension {virtual ~IServiceExtension()=default;
  virtual void on_service_event(const ServiceEvent&) noexcept=0;};
struct ITraceView {virtual ~ITraceView()=default;
  virtual std::string trace_json()const=0;};
} // namespace anaiero::contracts
