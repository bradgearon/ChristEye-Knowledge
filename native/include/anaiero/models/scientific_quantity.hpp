#pragma once
// Provenance-aware, exact integer units for future scientific couplers.
// No conversion from abstract proxies to physical measurements is legal.
#include <array>
#include <cstdint>
namespace anaiero::science {
enum class Unit : std::uint8_t {
  dimensionless_proxy=1,microvolt,millivolt,nanosecond,microsecond,
  millisecond,micromolar,millimolar
};
struct Quantity {
  std::int64_t value=0,uncertainty=0;
  Unit unit=Unit::dimensionless_proxy;
  std::uint32_t source=0;
  std::uint64_t epoch=0;
  std::array<char,65> model_sha256{};
};
struct ConservationBudget {
  Quantity before{},inflow{},outflow{},after{};
  std::int64_t absolute_tolerance=0;
};
struct QuantityIdentity {
  std::uint32_t source=0;
  std::uint64_t epoch=0;
  std::array<char,65> model_sha256{};
};
// Caller-declared, immutable coupling definition. Each term is pinned in
// before/inflow/outflow/after order; the transform digest identifies the
// reviewed mapping. This record is provenance, not proof of model validity.
struct ConservationContext {
  std::array<QuantityIdentity,4> terms{};
  std::array<char,65> transform_sha256{};
};
} // namespace anaiero::science
