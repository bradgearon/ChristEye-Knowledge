# CE-SW-36: native environment, model-qualified quantities and frozen fidelity.
# Included after NativeSessions, which defines anaiero_native_target.
anaiero_native_target(ce_environment_solver_provider examples/environment_solver_provider.cpp)
anaiero_native_target(ce_scientific_calibrate examples/scientific_calibration.cpp)
install(TARGETS ce_environment_solver_provider ce_scientific_calibrate RUNTIME DESTINATION bin)
if(TARGET anaiero)
  add_dependencies(anaiero ce_environment_solver_provider ce_scientific_calibrate)
endif()

if(CE_WIND_BUILD_TESTS)
  foreach(part environment_model environment_session scientific_calibration)
    anaiero_native_target(ce_${part}_tests tests/${part}_tests.cpp)
  endforeach()
  add_test(NAME anaiero_environment_model COMMAND ce_environment_model_tests
    $<TARGET_FILE:ce_environment_solver_provider>
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/scientific/well-mixed-environment.cevmodel
    ${CMAKE_CURRENT_BINARY_DIR}/environment-model-tests)
  add_test(NAME anaiero_environment_session COMMAND ce_environment_session_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    $<TARGET_FILE:ce_scientific_solver_provider>
    $<TARGET_FILE:ce_environment_solver_provider>
    ${CMAKE_CURRENT_BINARY_DIR}/environment-session-tests)
  add_test(NAME anaiero_scientific_calibration COMMAND ce_scientific_calibration_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_BINARY_DIR}/scientific-calibration-tests)
  add_test(NAME anaiero_environment_cli COMMAND ${CMAKE_COMMAND}
    -DCE_CONFIG=$<TARGET_FILE:ce_scientific_config>
    -DCE_PROVIDER=$<TARGET_FILE:ce_environment_solver_provider>
    -DCE_PASSIVE=$<TARGET_FILE:ce_scientific_solver_provider>
    -DCE_RUNTIME=$<TARGET_FILE:ce_colony_runtime>
    -DCE_CALIBRATE=$<TARGET_FILE:ce_scientific_calibrate>
    -DCE_REPO=${CMAKE_CURRENT_SOURCE_DIR}/..
    -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/environment-cli
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/environment_cli_smoke.cmake)
  set_tests_properties(anaiero_environment_model anaiero_environment_session
    anaiero_scientific_calibration anaiero_environment_cli PROPERTIES TIMEOUT 180)
endif()
