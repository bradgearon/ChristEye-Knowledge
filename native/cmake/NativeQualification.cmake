# The measurement executable is a native operator tool, not a dependency of the
# production runtime. Profiles pin numeric gates before the workload begins.
add_library(anaiero_qualification STATIC
  src/qualification/profile.cpp src/qualification/native_resources.cpp
  src/qualification/runner.cpp src/qualification/device.cpp src/qualification/mixed_profile.cpp
  src/qualification/mixed_runner.cpp src/qualification/mixed_campaign.cpp
  src/qualification/mixed_campaign_codec.cpp src/qualification/mixed_campaign_filesystem.cpp
  src/qualification/scale_profile.cpp src/qualification/scale_runner.cpp)
anaiero_sdk_target(anaiero_qualification)
target_include_directories(anaiero_qualification PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anaiero_qualification PRIVATE anaiero_lifecycle anaiero_storage_native anaiero_native_session anaiero_engine_files anaiero_mesh_service)
target_compile_definitions(anaiero_qualification PRIVATE ${anaiero_builtin_definitions})
if(WIN32)
  target_link_libraries(anaiero_qualification PRIVATE psapi)
endif()
add_executable(ce_native_qualification examples/native_qualification_cli.cpp)
anaiero_sdk_target(ce_native_qualification)
target_include_directories(ce_native_qualification PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/qualification)
target_link_libraries(ce_native_qualification PRIVATE anaiero_qualification anaiero_engine_files)
install(TARGETS ce_native_qualification RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/../qualification/native/
  DESTINATION ${CMAKE_INSTALL_DATADIR}/anaiero/qualification)
function(anaiero_attach_qualification_cli)
  if(TARGET anaiero)
    add_dependencies(anaiero ce_native_qualification)
  endif()
endfunction()
cmake_language(DEFER CALL anaiero_attach_qualification_cli)
if(CE_WIND_BUILD_TESTS)
  add_test(NAME anaiero_qualification_exact COMMAND ${CMAKE_COMMAND}
    -DANAIERO_QUALIFY=$<TARGET_FILE:ce_native_qualification>
    -DANAIERO_PROFILE=${CMAKE_CURRENT_SOURCE_DIR}/../qualification/native/ci-smoke.cevqualification
    -DANAIERO_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/qualification-exact
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/qualification_exact_smoke.cmake)
  set_tests_properties(anaiero_qualification_exact PROPERTIES TIMEOUT 360 LABELS "qualification")
  add_executable(anaiero_histogram_tests tests/qualification_histogram_tests.cpp)
  anaiero_sdk_target(anaiero_histogram_tests)
  target_include_directories(anaiero_histogram_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/qualification)
  target_link_libraries(anaiero_histogram_tests PRIVATE anaiero_qualification)
  add_test(NAME anaiero_qualification_histogram COMMAND anaiero_histogram_tests)
  set_tests_properties(anaiero_qualification_histogram PROPERTIES LABELS "qualification")
  add_test(NAME anaiero_mixed_qualification COMMAND ${CMAKE_COMMAND}
    -DANAIERO_QUALIFY=$<TARGET_FILE:ce_native_qualification>
    -DANAIERO_PROFILE=${CMAKE_CURRENT_SOURCE_DIR}/../qualification/native/mixed-ci.cevqualification
    -DANAIERO_SESSION=${CMAKE_CURRENT_SOURCE_DIR}/../examples/runtime/modular-native.cevsession
    -DANAIERO_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/mixed-qualification
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/mixed_qualification_smoke.cmake)
  set_tests_properties(anaiero_mixed_qualification PROPERTIES TIMEOUT 300 LABELS "qualification;mixed")
  add_test(NAME anaiero_qualification_profile COMMAND ${CMAKE_COMMAND}
    -DANAIERO_QUALIFY=$<TARGET_FILE:ce_native_qualification>
    -DANAIERO_PROFILE=${CMAKE_CURRENT_SOURCE_DIR}/../qualification/native/ci-smoke.cevqualification
    -DANAIERO_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/qualification-smoke
    -DANAIERO_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DANAIERO_LAUNCHER=${CMAKE_CURRENT_SOURCE_DIR}/cmake/RunNativeQualification.cmake
    -DANAIERO_CONFIG=$<CONFIG>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/qualification_smoke.cmake)
  set_tests_properties(anaiero_qualification_profile PROPERTIES TIMEOUT 180 LABELS "qualification")
  add_test(NAME anaiero_native_package COMMAND ${CMAKE_COMMAND}
    -DANAIERO_BUILD_DIR=${CMAKE_CURRENT_BINARY_DIR}
    -DANAIERO_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}
    -DANAIERO_TEST_ROOT=${CMAKE_CURRENT_BINARY_DIR}/native-package-tests
    -DANAIERO_CONFIG=$<CONFIG>
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/native_package_tests.cmake)
  set_tests_properties(anaiero_native_package PROPERTIES TIMEOUT 360 LABELS "qualification;package")
endif()

if(CE_WIND_BUILD_TESTS)
  add_executable(anaiero_scale_tests tests/scale_qualification_tests.cpp)
  anaiero_sdk_target(anaiero_scale_tests)
  target_include_directories(anaiero_scale_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/qualification)
  target_link_libraries(anaiero_scale_tests PRIVATE anaiero_qualification anaiero_storage_native anaiero_lifecycle anaiero_mesh_service)
  add_test(NAME anaiero_scale_qualification COMMAND anaiero_scale_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/scale-qualification-tests)
  set_tests_properties(anaiero_scale_qualification PROPERTIES TIMEOUT 300 LABELS "qualification;scale;lifecycle")
endif()

if(CE_WIND_BUILD_TESTS)
  add_executable(anaiero_mixed_campaign_tests tests/mixed_campaign_tests.cpp)
  anaiero_sdk_target(anaiero_mixed_campaign_tests)
  target_include_directories(anaiero_mixed_campaign_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/qualification)
  target_link_libraries(anaiero_mixed_campaign_tests PRIVATE anaiero_qualification anaiero_native_session anaiero_engine_files)
  add_test(NAME anaiero_mixed_campaign COMMAND anaiero_mixed_campaign_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/mixed-campaign-tests)
  set_tests_properties(anaiero_mixed_campaign PROPERTIES TIMEOUT 300 LABELS "qualification;mixed;recovery")
endif()

if(CE_WIND_BUILD_TESTS)
  # Actual exhaustion must use a purpose-built tiny mount/VHD from the explicit
  # platform launchers. Ordinary CTest verifies only the refusal boundary.
  add_executable(anaiero_storage_pressure_tests tests/storage_pressure_tests.cpp)
  anaiero_sdk_target(anaiero_storage_pressure_tests)
  target_compile_definitions(anaiero_storage_pressure_tests PRIVATE ${anaiero_builtin_definitions})
  target_link_libraries(anaiero_storage_pressure_tests PRIVATE anaiero_storage_native anaiero_engine_files)
  add_test(NAME anaiero_storage_pressure_guard COMMAND ${CMAKE_COMMAND}
    -DANAIERO_PRESSURE=$<TARGET_FILE:anaiero_storage_pressure_tests>
    -DANAIERO_GUARD_ROOT=${CMAKE_CURRENT_BINARY_DIR}/storage-pressure-guard
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/storage_pressure_guard.cmake)
  set_tests_properties(anaiero_storage_pressure_guard PROPERTIES TIMEOUT 30 LABELS "qualification;pressure")
endif()
