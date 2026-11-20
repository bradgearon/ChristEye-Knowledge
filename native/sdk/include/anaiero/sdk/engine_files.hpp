#pragma once
// Optional native file admission adapter. The portable SDK accepts owned bytes.
#include "anaiero/sdk/engines.hpp"
#include "anaiero/sdk/session_runner.hpp"
namespace anaiero::engines {
sdk::Result read_cell_configuration(std::string_view atlas_path,std::string_view world_path,sdk::Envelope&);
sdk::Result read_colony_configuration(std::string_view model_path,sdk::Envelope&);
sdk::Result read_plan_configuration(std::string_view plan_path,std::string_view base_path,
                                    std::string_view asset_root,sdk::Envelope&);
sdk::Result read_session_definition(std::string_view configuration_path,
                                    const sdk::EngineModuleCatalog&,sdk::SessionDefinition&);
// Publish complete bytes without replacing an existing destination. An exact
// existing match is an idempotent retry. A conflicting pending file is kept.
sdk::Result write_new_artifact(std::string_view path,std::string_view bytes);
}
