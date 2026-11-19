# The measurement executable is a native operator tool, not a dependency of the
# production runtime. Profiles pin numeric gates before the workload begins.
add_library(anaiero_qualification STATIC
  src/qualification/profile.cpp src/qualification/native_resources.cpp
  src/qualification/runner.cpp src/qualification/mixed_profile.cpp
  src/qualification/mixed_runner.cpp)
anaiero_sdk_target(anaiero_qualification)
target_link_libraries(anaiero_qualification PRIVATE anaiero_lifecycle anaiero_storage_native anaiero_native_session anaiero_engine_files)
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
