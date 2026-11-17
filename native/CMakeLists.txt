cmake_minimum_required(VERSION 3.20)
project(ChristEyeAnaieroNative LANGUAGES CXX)
option(CE_WIND_BUILD_TESTS "Build native WindBus codec/scheduler tests" ON)
option(ANAIERO_CONTROLLER_ONLY "Build the native session controller without demo/legacy CLIs" OFF)
option(ANAIERO_ENABLE_MESH "Build the optional OpenSSL 3 native authenticated mesh" OFF)

# Core header-only C++20 link targets; optional host adapters are declared below.
add_library(anaiero_models INTERFACE)
add_library(anaiero_contracts INTERFACE)
add_library(anaiero_domain INTERFACE)
add_library(anaiero_simulators INTERFACE)
add_library(anaiero_infrastructure INTERFACE)
add_library(anaiero_application INTERFACE)
foreach(tgt anaiero_models anaiero_contracts anaiero_domain anaiero_simulators
            anaiero_infrastructure anaiero_application)
  target_include_directories(${tgt} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_compile_features(${tgt} INTERFACE cxx_std_20)
endforeach()
target_link_libraries(anaiero_contracts INTERFACE anaiero_models)
target_link_libraries(anaiero_domain INTERFACE anaiero_contracts)
target_link_libraries(anaiero_simulators INTERFACE anaiero_domain)
target_link_libraries(anaiero_infrastructure INTERFACE anaiero_contracts)
target_link_libraries(anaiero_application INTERFACE anaiero_domain anaiero_simulators
  anaiero_infrastructure)

add_library(ce_wind_core INTERFACE)
target_include_directories(ce_wind_core INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_features(ce_wind_core INTERFACE cxx_std_20)
target_link_libraries(ce_wind_core INTERFACE anaiero_application)

if(ANAIERO_CONTROLLER_ONLY)
  find_package(Threads REQUIRED)
  add_executable(ce_colony_runtime examples/colony_runtime_cli.cpp examples/controller_host_cli.cpp examples/session_host_cli.cpp)
  target_link_libraries(ce_colony_runtime PRIVATE anaiero_application Threads::Threads)
  if(WIN32)
    target_link_libraries(ce_colony_runtime PRIVATE advapi32)
  endif()
  if(MSVC)
    target_compile_options(ce_colony_runtime PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_colony_runtime PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  install(TARGETS ce_colony_runtime RUNTIME DESTINATION bin)
  if(CE_WIND_BUILD_TESTS)
    enable_testing()
  endif()
  include(cmake/NativeSessions.cmake)
  include(cmake/NativeMesh.cmake)
  return()
endif()

# SIM1 demo is part of the installed runtime, even with tests disabled.
add_executable(ce_living_sim_demo examples/living_sim_demo.cpp)
target_link_libraries(ce_living_sim_demo PRIVATE ce_wind_core)
if(CE_WIND_BUILD_TESTS)
  enable_testing()
  add_executable(ce_wind_core_tests tests/core_tests.cpp)
  target_link_libraries(ce_wind_core_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_wind_core_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_wind_core_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME wind_core_tests COMMAND ce_wind_core_tests)
  add_executable(ce_wind_loopback examples/loopback.cpp)
  target_link_libraries(ce_wind_loopback PRIVATE ce_wind_core)
  add_test(NAME wind_loopback COMMAND ce_wind_loopback)
  add_executable(ce_wind_media_tests tests/media_patchbay_tests.cpp)
  target_link_libraries(ce_wind_media_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_wind_media_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_wind_media_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME wind_media_tests COMMAND ce_wind_media_tests)
  add_executable(ce_living_sim_tests tests/living_sim_tests.cpp)
  target_link_libraries(ce_living_sim_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_living_sim_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_living_sim_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME living_sim_tests COMMAND ce_living_sim_tests)
  add_test(NAME living_sim_demo COMMAND ce_living_sim_demo)
  add_executable(ce_vlm_tests tests/virtual_machine_tests.cpp)
  target_link_libraries(ce_vlm_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_vlm_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_vlm_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME vlm_file_replay COMMAND ce_vlm_tests)
  add_test(NAME vlm_template COMMAND ce_vlm inspect ${CMAKE_CURRENT_SOURCE_DIR}/../examples/vlm/crest-garden.avlm)
endif()

# Host-only VLM CLI, separate from freestanding SIM1 simulator header.
add_executable(ce_vlm examples/vlm_cli.cpp)
target_link_libraries(ce_vlm PRIVATE ce_wind_core)
if(MSVC)
  target_compile_options(ce_vlm PRIVATE /W4 /WX)
else()
  target_compile_options(ce_vlm PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()

# VLM2 host process/visual timeline (kept separate from freestanding SIM1).
add_executable(ce_vlm_process examples/vlm_process_cli.cpp)
target_link_libraries(ce_vlm_process PRIVATE ce_wind_core)
if(MSVC)
  target_compile_options(ce_vlm_process PRIVATE /W4 /WX)
else()
  target_compile_options(ce_vlm_process PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_vlm_process_tests tests/process_machine_tests.cpp)
  target_link_libraries(ce_vlm_process_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_vlm_process_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_vlm_process_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME vlm_process_tests COMMAND ce_vlm_process_tests)
  add_test(NAME vlm_process_template COMMAND ce_vlm_process inspect ${CMAKE_CURRENT_SOURCE_DIR}/../examples/vlm/process-garden.avlm)
endif()

# CE-SW-09 host-only content-addressed learning and Crystal/RNA plan runner.
add_executable(ce_vlm_plan examples/vlm_plan_cli.cpp)
target_link_libraries(ce_vlm_plan PRIVATE ce_wind_core)
if(MSVC)
  target_compile_options(ce_vlm_plan PRIVATE /W4 /WX)
else()
  target_compile_options(ce_vlm_plan PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_vlm_plan_tests tests/learning_plan_tests.cpp)
  target_link_libraries(ce_vlm_plan_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_vlm_plan_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_vlm_plan_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME vlm_learning_plan_tests COMMAND ce_vlm_plan_tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME vlm_learning_plan_template COMMAND ce_vlm_plan inspect
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/vlm/bread-first.cevplan
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/vlm/plan-garden.avlm
    ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# CE-SW-10 host-only WindStore-compatible content-addressed registry.
add_executable(ce_wind_registry examples/wind_registry_cli.cpp)
target_link_libraries(ce_wind_registry PRIVATE ce_wind_core)
if(MSVC)
  target_compile_options(ce_wind_registry PRIVATE /W4 /WX)
else()
  target_compile_options(ce_wind_registry PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_wind_registry_tests tests/wind_registry_tests.cpp)
  target_link_libraries(ce_wind_registry_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_wind_registry_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_wind_registry_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME wind_registry_tests COMMAND ce_wind_registry_tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# CE-SW-11 SW2.1: crash-recoverable host WindStore and persistent cursors.
add_executable(ce_wind_durable examples/wind_durable_cli.cpp)
target_link_libraries(ce_wind_durable PRIVATE ce_wind_core)
target_compile_definitions(ce_wind_durable PRIVATE CE_DURABLE_WITH_LEARNING_PLAN)
if(MSVC)
  target_compile_options(ce_wind_durable PRIVATE /W4 /WX)
else()
  target_compile_options(ce_wind_durable PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_wind_durable_tests tests/durable_registry_tests.cpp)
  target_link_libraries(ce_wind_durable_tests PRIVATE ce_wind_core)
  target_compile_definitions(ce_wind_durable_tests PRIVATE CE_DURABLE_FAULT_TESTS)
  if(MSVC)
    target_compile_options(ce_wind_durable_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_wind_durable_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME wind_durable_tests COMMAND ce_wind_durable_tests)
endif()

# SW2.1 full CLI roundtrip: import -> publish -> replay -> ack -> verify.
if(CE_WIND_BUILD_TESTS)
  add_test(NAME wind_durable_cli_smoke COMMAND ${CMAKE_COMMAND}
    -DCE_CLI=$<TARGET_FILE:ce_wind_durable>
    -DCE_TMP=${CMAKE_CURRENT_BINARY_DIR}/durable-cli-smoke
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/durable_cli_smoke.cmake)
endif()

# CE-SW-12 SIM3 persistent multi-colony runner and 3D view.
add_executable(ce_colony_runner examples/colony_runner_cli.cpp)
target_link_libraries(ce_colony_runner PRIVATE ce_wind_core)
if(MSVC)
  target_compile_options(ce_colony_runner PRIVATE /W4 /WX)
else()
  target_compile_options(ce_colony_runner PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_colony_runner_tests tests/colony_runner_tests.cpp)
  target_link_libraries(ce_colony_runner_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_colony_runner_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_colony_runner_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME sim3_colony_runner COMMAND ce_colony_runner_tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME sim3_example_template COMMAND ce_colony_runner inspect
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/connected-gardens.cev
    ${CMAKE_CURRENT_BINARY_DIR}/sim3-template-smoke)
endif()

# CE-SW-13 SIM3.1 Unified Living Laboratory
add_executable(ce_colony_lab examples/colony_lab_cli.cpp)
target_link_libraries(ce_colony_lab PRIVATE ce_wind_core)
if(MSVC)
  target_compile_options(ce_colony_lab PRIVATE /W4 /WX)
else()
  target_compile_options(ce_colony_lab PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_colony_lab_tests tests/colony_lab_tests.cpp)
  target_link_libraries(ce_colony_lab_tests PRIVATE ce_wind_core)
  add_executable(ce_colony_overlay_tests tests/colony_overlay_tests.cpp)
  target_link_libraries(ce_colony_overlay_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_colony_lab_tests PRIVATE /W4 /WX)
    target_compile_options(ce_colony_overlay_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_colony_lab_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
    target_compile_options(ce_colony_overlay_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME colony_lab_tests COMMAND ce_colony_lab_tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME colony_overlay_tests COMMAND ce_colony_overlay_tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
endif()

# CE-SW-14 SIM3.2 deterministic, read-only synthetic model fidelity evaluation.
add_executable(ce_colony_fidelity examples/colony_fidelity_cli.cpp)
target_link_libraries(ce_colony_fidelity PRIVATE ce_wind_core)
if(MSVC)
  target_compile_options(ce_colony_fidelity PRIVATE /W4 /WX)
else()
  target_compile_options(ce_colony_fidelity PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_colony_fidelity_tests tests/colony_fidelity_tests.cpp)
  target_link_libraries(ce_colony_fidelity_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_colony_fidelity_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_colony_fidelity_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME colony_fidelity_tests COMMAND ce_colony_fidelity_tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME colony_fidelity_smoke COMMAND ${CMAKE_COMMAND}
    -DCE_RUNNER=$<TARGET_FILE:ce_colony_runner>
    -DCE_FIDELITY=$<TARGET_FILE:ce_colony_fidelity>
    -DCE_REPO=${CMAKE_CURRENT_SOURCE_DIR}/..
    -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/sim32-cli-smoke
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/colony_fidelity_smoke.cmake)
endif()


# CE-SW-15 SIM3.3 — training-only model selection and frozen final-test evaluation.
add_executable(ce_colony_select examples/colony_selection_cli.cpp)
target_link_libraries(ce_colony_select PRIVATE ce_wind_core)
if(MSVC)
  target_compile_options(ce_colony_select PRIVATE /W4 /WX)
else()
  target_compile_options(ce_colony_select PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_colony_selection_tests tests/colony_selection_tests.cpp)
  target_link_libraries(ce_colony_selection_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_colony_selection_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_colony_selection_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME sim33_selection_integration COMMAND ${CMAKE_COMMAND}
    -DCE_RUNNER=$<TARGET_FILE:ce_colony_runner>
    -DCE_SELECT=$<TARGET_FILE:ce_colony_select>
    -DCE_TEST=$<TARGET_FILE:ce_colony_selection_tests>
    -DCE_REPO=${CMAKE_CURRENT_SOURCE_DIR}/..
    -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/sim33-smoke
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/colony_selection_smoke.cmake)
endif()

# CE-SW-17 public native Anaiero CLI; historical ce_* executables are internal.
add_executable(anaiero examples/anaiero_cli.cpp)
target_compile_features(anaiero PRIVATE cxx_std_20)
if(MSVC)
  target_compile_options(anaiero PRIVATE /W4 /WX)
else()
  target_compile_options(anaiero PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
set(ANA_MODULES
  ce_vlm ce_vlm_process ce_vlm_plan ce_wind_registry ce_wind_durable
  ce_colony_runner ce_colony_lab ce_colony_fidelity ce_colony_select
  ce_living_sim_demo
)
add_dependencies(anaiero ${ANA_MODULES})
install(TARGETS anaiero ${ANA_MODULES} RUNTIME DESTINATION bin)
if(CE_WIND_BUILD_TESTS)
  add_test(NAME anaiero_dispatch_smoke COMMAND ${CMAKE_COMMAND}
    -DCE_ANA=$<TARGET_FILE:anaiero>
    -DCE_REPO=${CMAKE_CURRENT_SOURCE_DIR}/..
    -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/anaiero-smoke
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/anaiero_dispatch_smoke.cmake)
endif()

# CE-SW-18 low-resource native controller ColonyState projector and weighted Animon work.
find_package(Threads REQUIRED)
add_executable(ce_colony_runtime examples/colony_runtime_cli.cpp examples/controller_host_cli.cpp examples/session_host_cli.cpp)
target_link_libraries(ce_colony_runtime PRIVATE ce_wind_core Threads::Threads)
if(MSVC)
  target_compile_options(ce_colony_runtime PRIVATE /W4 /WX)
else()
  target_compile_options(ce_colony_runtime PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_colony_runtime_tests tests/colony_runtime_tests.cpp)
  target_link_libraries(ce_colony_runtime_tests PRIVATE ce_wind_core Threads::Threads)
  if(MSVC)
    target_compile_options(ce_colony_runtime_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_colony_runtime_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_runtime_projector COMMAND ce_colony_runtime_tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME anaiero_runtime_simulate COMMAND ce_colony_runtime simulate
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/connected-gardens.cev
    ${CMAKE_CURRENT_BINARY_DIR}/anaiero-runtime-simulated 2)
endif()

# The public anaiero target must build/install every listed backend.
add_dependencies(anaiero ce_colony_runtime)
install(TARGETS ce_colony_runtime RUNTIME DESTINATION bin)

# CE-SW-19 evidence-linked, synthetic-only cell-behavior atlas and runner.
add_executable(ce_cell_behavior examples/cell_behavior_cli.cpp)
target_link_libraries(ce_cell_behavior PRIVATE ce_wind_core Threads::Threads)
if(MSVC)
  target_compile_options(ce_cell_behavior PRIVATE /W4 /WX)
else()
  target_compile_options(ce_cell_behavior PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_cell_behavior_tests tests/cell_behavior_tests.cpp)
  target_link_libraries(ce_cell_behavior_tests PRIVATE ce_wind_core)
  add_executable(ce_cell_behavior_bus_tests tests/cell_behavior_bus_tests.cpp)
  target_link_libraries(ce_cell_behavior_bus_tests PRIVATE ce_wind_core Threads::Threads)
  if(MSVC)
    target_compile_options(ce_cell_behavior_tests PRIVATE /W4 /WX)
    target_compile_options(ce_cell_behavior_bus_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_cell_behavior_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
    target_compile_options(ce_cell_behavior_bus_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_cells_profile_replay COMMAND ce_cell_behavior_tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME anaiero_cells_windbus COMMAND ce_cell_behavior_bus_tests ${CMAKE_CURRENT_SOURCE_DIR}/..)
  add_test(NAME anaiero_cells_cli_smoke COMMAND ${CMAKE_COMMAND}
    -DCE_CELLS=$<TARGET_FILE:ce_cell_behavior>
    -DCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/..
    -DCE_TMP=${CMAKE_CURRENT_BINARY_DIR}/sw5-cells-smoke
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/cell_behavior_cli_smoke.cmake)
endif()
add_dependencies(anaiero ce_cell_behavior)
install(TARGETS ce_cell_behavior RUNTIME DESTINATION bin)

# Anaiero v0.015 adaptive optimization: pure service scoring + demo route adapter.
add_executable(ce_optimization examples/optimization_cli.cpp examples/optimization_fork_cli.cpp)
target_link_libraries(ce_optimization PRIVATE ce_wind_core Threads::Threads)
if(MSVC)
  target_compile_options(ce_optimization PRIVATE /W4 /WX)
else()
  target_compile_options(ce_optimization PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
add_dependencies(anaiero ce_optimization)
install(TARGETS ce_optimization RUNTIME DESTINATION bin)
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_adaptive_optimization_tests tests/adaptive_optimization_tests.cpp)
  target_link_libraries(ce_adaptive_optimization_tests PRIVATE ce_wind_core Threads::Threads)
  if(MSVC)
    target_compile_options(ce_adaptive_optimization_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_adaptive_optimization_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_adaptive_optimization_unit COMMAND ce_adaptive_optimization_tests)
  add_test(NAME anaiero_adaptive_optimization_cli COMMAND ce_optimization demo)
endif()

# Real SW5 deterministic forks, multi-goal optimization, CAS-pinned route receipt.
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_fork_optimizer_tests tests/fork_optimizer_tests.cpp)
  target_link_libraries(ce_fork_optimizer_tests PRIVATE ce_wind_core Threads::Threads)
  if(MSVC)
    target_compile_options(ce_fork_optimizer_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_fork_optimizer_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_sw5_fork_optimizer
    COMMAND ce_fork_optimizer_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
    ${CMAKE_CURRENT_BINARY_DIR}/sw5-fork-session)
endif()

# CE-SW-23: real outbox receipts through WindPacket, durable WindStore and SW4 projector.
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_route_delivery_tests tests/route_delivery_tests.cpp)
  target_link_libraries(ce_route_delivery_tests PRIVATE ce_wind_core Threads::Threads)
  if(MSVC)
    target_compile_options(ce_route_delivery_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_route_delivery_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_route_delivery COMMAND ce_route_delivery_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
    ${CMAKE_CURRENT_BINARY_DIR}/route-delivery-integration)
  add_test(NAME anaiero_route_delivery_cli COMMAND ${CMAKE_COMMAND}
    -DCE_CLI=$<TARGET_FILE:ce_optimization>
    -DCE_ATLAS=${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
    -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/route-delivery-cli
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/route_delivery_smoke.cmake)
endif()

# CE-SW-24: lightweight native scientific-model coupling and hypothesis runtime.
# Third-party scientific engines are OPTIONAL outside the Anaiero controller.
add_executable(ce_science_lab
  examples/scientific_lab_cli.cpp
  examples/scientific_bus_cli.cpp
  examples/scientific_live_cli.cpp
  examples/scientific_epoch_cli.cpp)
target_link_libraries(ce_science_lab PRIVATE ce_wind_core Threads::Threads)
if(MSVC)
  target_compile_options(ce_science_lab PRIVATE /W4 /WX)
else()
  target_compile_options(ce_science_lab PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
add_dependencies(anaiero ce_science_lab)
install(TARGETS ce_science_lab RUNTIME DESTINATION bin)
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_scientific_coupling_tests tests/scientific_coupling_tests.cpp)
  add_executable(ce_scientific_trace_tests tests/scientific_trace_tests.cpp)
  target_link_libraries(ce_scientific_coupling_tests PRIVATE ce_wind_core)
  target_link_libraries(ce_scientific_trace_tests PRIVATE ce_wind_core)
  if(MSVC)
    target_compile_options(ce_scientific_coupling_tests PRIVATE /W4 /WX)
    target_compile_options(ce_scientific_trace_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_scientific_coupling_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
    target_compile_options(ce_scientific_trace_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_science_coupling COMMAND ce_scientific_coupling_tests)
  add_test(NAME anaiero_science_trace COMMAND ce_scientific_trace_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/scientific/native-reference.cevscitrace)
  add_test(NAME anaiero_science_cli COMMAND ${CMAKE_COMMAND}
    -DCE_SCI=$<TARGET_FILE:ce_science_lab>
    -DCE_ANA=$<TARGET_FILE:anaiero>
    -DCE_ATLAS=${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
    -DCE_TRACE=${CMAKE_CURRENT_SOURCE_DIR}/../examples/scientific/native-reference.cevscitrace
    -DCE_ENV_TRACE=${CMAKE_CURRENT_SOURCE_DIR}/../examples/scientific/native-environment.cevscitrace
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/scientific_cli_smoke.cmake)
endif()

# CE-SW-25/26: native, epoch-aware recovery and live external-solver stepping.
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_epoch_observation_tests tests/epoch_observation_tests.cpp)
  add_executable(ce_live_solver_tests tests/live_solver_tests.cpp)
  target_link_libraries(ce_epoch_observation_tests PRIVATE ce_wind_core Threads::Threads)
  target_link_libraries(ce_live_solver_tests PRIVATE ce_wind_core Threads::Threads)
  if(MSVC)
    target_compile_options(ce_epoch_observation_tests PRIVATE /W4 /WX)
    target_compile_options(ce_live_solver_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_epoch_observation_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
    target_compile_options(ce_live_solver_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_epoch_observations COMMAND ce_epoch_observation_tests
    ${CMAKE_CURRENT_BINARY_DIR}/epoch-observations)
  add_test(NAME anaiero_live_solver_contract COMMAND ce_live_solver_tests)
  add_test(NAME anaiero_epoch_replay_cli COMMAND ${CMAKE_COMMAND}
    -DCE_ANA=$<TARGET_FILE:anaiero>
    -DCE_ATLAS=${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
    -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/epoch-replay-cli
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/scientific_epoch_smoke.cmake)
  # Python is only a TEST and optional scientific sidecar, not a native
  # runtime dependency. The basic C++ controller builds without it.
  find_package(Python3 COMPONENTS Interpreter QUIET)
  if(Python3_Interpreter_FOUND)
    add_test(NAME anaiero_live_solver_stdio_cli COMMAND ${CMAKE_COMMAND}
      -DCE_ANA=$<TARGET_FILE:anaiero>
      -DCE_PYTHON=${Python3_EXECUTABLE}
      -DCE_DRIVER=${CMAKE_CURRENT_SOURCE_DIR}/../tools/scientific/live_solver_bridge.py
      -DCE_ATLAS=${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
      -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/scientific-live-cli
      -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/scientific_live_smoke.cmake)
  endif()
endif()

# CE-SW-27 reusable simulation kernel, both native Windows and Linux.
add_executable(ce_simulation_kernel examples/simulation_kernel_cli.cpp)
target_link_libraries(ce_simulation_kernel PRIVATE ce_wind_core Threads::Threads)
if(MSVC)
  target_compile_options(ce_simulation_kernel PRIVATE /W4 /WX)
else()
  target_compile_options(ce_simulation_kernel PRIVATE -Wall -Wextra -Wconversion -Werror)
endif()
add_dependencies(anaiero ce_simulation_kernel)
install(TARGETS ce_simulation_kernel RUNTIME DESTINATION bin)
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_simulation_kernel_tests tests/simulation_kernel_tests.cpp)
  target_link_libraries(ce_simulation_kernel_tests PRIVATE ce_wind_core Threads::Threads)
  if(MSVC)
    target_compile_options(ce_simulation_kernel_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_simulation_kernel_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_engine_kernel COMMAND ce_simulation_kernel_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/connected-gardens.cev
    ${CMAKE_CURRENT_BINARY_DIR}/kernel-tests)
  add_test(NAME anaiero_engine_kernel_cli COMMAND ${CMAKE_COMMAND}
    -DCE_KERNEL=$<TARGET_FILE:ce_simulation_kernel>
    -DCE_ATLAS=${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
    -DCE_SIM3=${CMAKE_CURRENT_SOURCE_DIR}/../examples/connected-gardens.cev
    -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/kernel-cli
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/simulation_kernel_smoke.cmake)
endif()

# Native Windows/Linux same-user IPC server and weighted queue linear-scan regression.
if(WIN32)
  target_link_libraries(ce_colony_runtime PRIVATE advapi32)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_native_host_tests tests/native_host_tests.cpp)
  target_link_libraries(ce_native_host_tests PRIVATE ce_wind_core Threads::Threads)
  if(WIN32)
    target_link_libraries(ce_native_host_tests PRIVATE advapi32)
  endif()
  if(MSVC)
    target_compile_options(ce_native_host_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_native_host_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_native_host_ipc COMMAND ce_native_host_tests)
  set_tests_properties(anaiero_native_host_ipc PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90)
endif()

# CE-SW-31: one recoverable native session and an independently responsive host.
if(CE_WIND_BUILD_TESTS)
  foreach(part native_session session_record session_host_control)
    add_executable(ce_${part}_tests tests/${part}_tests.cpp)
    target_link_libraries(ce_${part}_tests PRIVATE ce_wind_core Threads::Threads)
    if(MSVC)
      target_compile_options(ce_${part}_tests PRIVATE /W4 /WX)
    else()
      target_compile_options(ce_${part}_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
    endif()
  endforeach()
  add_test(NAME anaiero_native_session COMMAND ce_native_session_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/runtime/mixed-native.cevsession
    ${CMAKE_CURRENT_BINARY_DIR}/native-session-tests)
  add_test(NAME anaiero_session_record COMMAND ce_session_record_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/session-record-tests)
  add_test(NAME anaiero_session_host_control COMMAND ce_session_host_control_tests)
  set_tests_properties(anaiero_native_session anaiero_session_record
    anaiero_session_host_control PROPERTIES TIMEOUT 120)
  add_test(NAME anaiero_session_cli COMMAND ${CMAKE_COMMAND}
    -DCE_RUNTIME=$<TARGET_FILE:ce_colony_runtime>
    -DCE_CONFIG=${CMAKE_CURRENT_SOURCE_DIR}/../examples/runtime/mixed-native.cevsession
    -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/session-cli
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/native_session_cli_smoke.cmake)
endif()

# CE-SW-29: one bounded async persistence worker, checkpoint indexes, units.
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_async_evidence_tests tests/async_evidence_tests.cpp)
  target_link_libraries(ce_async_evidence_tests PRIVATE ce_wind_core Threads::Threads)
  if(MSVC)
    target_compile_options(ce_async_evidence_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_async_evidence_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_async_evidence_and_units COMMAND ce_async_evidence_tests
    ${CMAKE_CURRENT_BINARY_DIR}/async-evidence-session)
endif()

# CE-SW-30: engine-neutral optimizer with real SW5 matched-horizon forks.
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_engine_optimization_tests tests/engine_optimization_tests.cpp)
  target_link_libraries(ce_engine_optimization_tests PRIVATE ce_wind_core Threads::Threads)
  if(MSVC)
    target_compile_options(ce_engine_optimization_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(ce_engine_optimization_tests PRIVATE
      -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_engine_optimization COMMAND ce_engine_optimization_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
    ${CMAKE_CURRENT_BINARY_DIR}/engine-optimization-session)
endif()

include(cmake/NativeSessions.cmake)
include(cmake/NativeMesh.cmake)
