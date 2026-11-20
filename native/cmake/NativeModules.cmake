# CE-SW-37: installed, independently compiled SDK. Keep all concrete models,
# persistence mechanisms and platform headers outside public runtime modules.
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

function(anaiero_sdk_target target)
  target_include_directories(${target} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/sdk/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
  target_compile_features(${target} PUBLIC cxx_std_20)
  set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
endfunction()

add_library(anaiero_contracts STATIC src/sdk/contracts.cpp src/sdk/migration.cpp)
anaiero_sdk_target(anaiero_contracts)
add_library(anaiero_catalog STATIC src/sdk/catalog.cpp)
anaiero_sdk_target(anaiero_catalog)
target_link_libraries(anaiero_catalog PUBLIC anaiero_contracts)
add_library(anaiero_projection STATIC src/sdk/projection.cpp)
anaiero_sdk_target(anaiero_projection)
target_link_libraries(anaiero_projection PUBLIC anaiero_contracts)
add_library(anaiero_observers STATIC src/sdk/observer.cpp)
anaiero_sdk_target(anaiero_observers)
target_link_libraries(anaiero_observers PUBLIC anaiero_contracts)
add_library(anaiero_runtime STATIC src/sdk/runtime.cpp)
anaiero_sdk_target(anaiero_runtime)
target_link_libraries(anaiero_runtime PUBLIC anaiero_catalog anaiero_projection)
add_library(anaiero_experiments STATIC src/sdk/experiments.cpp)
anaiero_sdk_target(anaiero_experiments)
target_link_libraries(anaiero_experiments PUBLIC anaiero_contracts)
add_library(anaiero_module_api STATIC src/sdk/module_adapter.cpp src/sdk/module_stdio.cpp)
anaiero_sdk_target(anaiero_module_api)
target_link_libraries(anaiero_module_api PUBLIC anaiero_contracts)
add_library(anaiero_module_host STATIC src/sdk/module_proxy.cpp src/sdk/module_native.cpp src/sdk/module_process.cpp src/sdk/native_clock.cpp)
anaiero_sdk_target(anaiero_module_host)
target_include_directories(anaiero_module_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anaiero_module_host PUBLIC anaiero_catalog PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
if(WIN32)
  target_link_libraries(anaiero_module_host PRIVATE advapi32)
endif()

# These are separately owned compiled providers, not public include shortcuts.
include(${CMAKE_CURRENT_LIST_DIR}/NativeEvidence.cmake OPTIONAL)
if(TARGET anaiero_evidence)
  target_link_libraries(anaiero_runtime PRIVATE anaiero_evidence)
endif()
include(${CMAKE_CURRENT_LIST_DIR}/NativeLifecycle.cmake OPTIONAL)
include(${CMAKE_CURRENT_LIST_DIR}/NativeEngines.cmake OPTIONAL)
include(${CMAKE_CURRENT_LIST_DIR}/NativeHost.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/NativeContinuousMesh.cmake OPTIONAL)
include(${CMAKE_CURRENT_LIST_DIR}/NativeQualification.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/ModularSessionTests.cmake)

install(TARGETS anaiero_contracts anaiero_catalog anaiero_projection anaiero_runtime anaiero_experiments
  anaiero_module_api anaiero_module_host anaiero_observers
  EXPORT AnaieroTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(DIRECTORY sdk/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES docs/module-sdk.md docs/canonical-evidence.md docs/modular-lifecycle.md
  docs/sdk-reference.md docs/continuous-mesh.md DESTINATION ${CMAKE_INSTALL_DATADIR}/anaiero)
configure_package_config_file(${CMAKE_CURRENT_LIST_DIR}/AnaieroConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/AnaieroConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Anaiero)
write_basic_package_version_file(${CMAKE_CURRENT_BINARY_DIR}/AnaieroConfigVersion.cmake
  VERSION 1.0.0 COMPATIBILITY SameMajorVersion)
install(EXPORT AnaieroTargets NAMESPACE anaiero:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Anaiero)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/AnaieroConfig.cmake ${CMAKE_CURRENT_BINARY_DIR}/AnaieroConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Anaiero)
install(DIRECTORY tests/sdk_consumer/ DESTINATION ${CMAKE_INSTALL_DATADIR}/anaiero/examples/independent-sdk)

if(CE_WIND_BUILD_TESTS)
  # CE-SW-43: actual v1 -> v2 checkpoint conversion and committed recovery.
  add_executable(anaiero_engine_migration_tests tests/engine_migration_tests.cpp)
  anaiero_sdk_target(anaiero_engine_migration_tests)
  target_link_libraries(anaiero_engine_migration_tests PRIVATE anaiero_runtime anaiero_storage_native)
  add_test(NAME anaiero_engine_migration COMMAND anaiero_engine_migration_tests
    "${CMAKE_CURRENT_BINARY_DIR}/engine-migration-tests")
  set_tests_properties(anaiero_engine_migration PROPERTIES TIMEOUT 120 LABELS "migration;recovery")
  add_library(anaiero_independent_native MODULE tests/sdk_consumer/provider.cpp)
  anaiero_sdk_target(anaiero_independent_native)
  target_compile_definitions(anaiero_independent_native PRIVATE ANAIERO_MODULE_BUILD)
  target_link_libraries(anaiero_independent_native PRIVATE anaiero_module_api)
  add_library(anaiero_independent_invalid MODULE tests/sdk_consumer/invalid_module.cpp)
  anaiero_sdk_target(anaiero_independent_invalid)
  target_compile_definitions(anaiero_independent_invalid PRIVATE ANAIERO_MODULE_BUILD)
  target_link_libraries(anaiero_independent_invalid PRIVATE anaiero_contracts)
  add_executable(anaiero_independent_process tests/sdk_consumer/provider.cpp)
  anaiero_sdk_target(anaiero_independent_process)
  target_link_libraries(anaiero_independent_process PRIVATE anaiero_module_api)
  add_executable(anaiero_sdk_tests tests/sdk_consumer/consumer.cpp)
  anaiero_sdk_target(anaiero_sdk_tests)
  target_link_libraries(anaiero_sdk_tests PRIVATE anaiero_runtime anaiero_experiments anaiero_module_host anaiero_observers
    anaiero_storage_native Threads::Threads)
  add_test(NAME anaiero_sdk_conformance COMMAND anaiero_sdk_tests
    $<TARGET_FILE:anaiero_independent_native> $<TARGET_FILE:anaiero_independent_process>
    $<TARGET_FILE:anaiero_independent_invalid> ${CMAKE_CURRENT_BINARY_DIR}/sdk-conformance)
  add_test(NAME anaiero_sdk_installed_consumer COMMAND ${CMAKE_COMMAND}
    -DANAIERO_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DANAIERO_CONSUMER_SOURCE=${CMAKE_CURRENT_SOURCE_DIR}/tests/sdk_consumer
    -DANAIERO_TEST_CONFIG=$<CONFIG>
    -DANAIERO_CTEST=${CMAKE_CTEST_COMMAND}
    -P ${CMAKE_CURRENT_LIST_DIR}/RunInstalledSdk.cmake)
  set_tests_properties(anaiero_sdk_installed_consumer PROPERTIES TIMEOUT 240)
endif()
