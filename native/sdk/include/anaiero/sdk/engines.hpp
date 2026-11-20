#pragma once
// Built-in module admission and data packaging; no concrete model types escape.
#include "anaiero/sdk/catalog.hpp"
#include <span>
namespace anaiero::engines {
inline constexpr std::string_view cell_behavior="anaiero.cell-behavior/1";
inline constexpr std::string_view multi_colony="anaiero.multi-colony/1";
inline constexpr std::string_view vlm_plan="anaiero.vlm-plan/1";
inline constexpr std::string_view passive_membrane="anaiero.passive-membrane/1";
inline constexpr std::string_view environment="anaiero.environment/1";
struct RetainedAsset { std::string relative_path,bytes; };
// Each constructor verifies immutable model/source/asset bytes before admission.
sdk::Result cell_configuration(std::string_view atlas,std::string_view reference,
                               std::string_view initial_world,sdk::Envelope&);
sdk::Result colony_configuration(std::string_view model,
                                 std::span<const RetainedAsset>,sdk::Envelope&);
sdk::Result plan_configuration(std::string_view plan,std::string_view base,
                               std::span<const RetainedAsset>,sdk::Envelope&);
sdk::Result numerical_configuration(std::string_view engine_type,std::string_view model,
                                    std::uint64_t tick_ns,std::int32_t input,sdk::Envelope&);
sdk::Result register_builtin_engines(sdk::EngineModuleCatalog&);
sdk::Result identify(const sdk::EngineModuleCatalog&,std::string_view type,
                     std::string_view instance,std::string_view source,std::uint64_t epoch,
                     const sdk::Envelope&,sdk::EngineIdentity&);
// Canonical textual command schemas for operators and independent clients.
sdk::Envelope cell_input(std::int32_t support,std::int32_t stress,
                         std::int32_t signal,bool lesson);
sdk::Envelope no_input(std::string_view engine_type);
sdk::Envelope numerical_input(std::int32_t input);
// Export the unchanged CEVLM/2 image and original interpreter receipts.
sdk::Result plan_output(const sdk::EngineState&,std::string& image,
                        std::string& receipts_ndjson);
}
