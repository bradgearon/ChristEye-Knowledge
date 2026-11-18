#pragma once
// Declared scalar transport semantics, not a biological identity or calibration.
// Provider, model and behavior pins identify what is modeled (including species).
// Adding a unit here does not infer scientific meaning from arbitrary quantities.
#include "anaiero/contracts/simulation_engine.hpp"
#include "anaiero/models/colony_state.hpp"
#include "anaiero/models/scientific_quantity.hpp"
#include <array>
#include <cstdint>
#include <string_view>
namespace anaiero::science {
struct ScientificObservable {
  Unit unit;
  engine::SummarySemantic summary;
  runtime::Metric metric;
  std::uint32_t input_schema,metric_schema;
  std::string_view summary_name,unit_name;
};
inline constexpr std::array<ScientificObservable,2> kScientificObservables{{
  {Unit::microvolt,engine::SummarySemantic::membrane_voltage_microvolt,
   runtime::Metric::membrane_voltage,0xCE710001u,0xCE710002u,
   "membrane_voltage_microvolt","microvolt"},
  {Unit::micromolar,engine::SummarySemantic::concentration_micromolar,
   runtime::Metric::environmental_concentration,0xCE720001u,0xCE720002u,
   "concentration_micromolar","micromolar"}
}};
// Accept the raw value so parsers cannot truncate an invalid integer into a unit.
inline constexpr const ScientificObservable* observable_for_unit(std::uint32_t unit)noexcept {
  for(const auto& item:kScientificObservables)
    if(static_cast<std::uint32_t>(item.unit)==unit)return &item;
  return nullptr;
}
inline constexpr const ScientificObservable* observable_for_unit(Unit unit)noexcept {
  return observable_for_unit(static_cast<std::uint32_t>(unit));
}
inline constexpr const ScientificObservable* observable_for_summary(engine::SummarySemantic summary)noexcept {
  for(const auto& item:kScientificObservables)if(item.summary==summary)return &item;
  return nullptr;
}
inline constexpr const ScientificObservable* observable_for_metric(runtime::Metric metric)noexcept {
  for(const auto& item:kScientificObservables)if(item.metric==metric)return &item;
  return nullptr;
}
} // namespace anaiero::science
