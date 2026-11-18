include_guard(GLOBAL)
if(CE_WIND_BUILD_TESTS)
  add_executable(ce_modular_session_tests tests/modular_session_tests.cpp)
  anaiero_sdk_target(ce_modular_session_tests)
  target_link_libraries(ce_modular_session_tests PRIVATE anaiero_engine_files anaiero_session_runner
    anaiero_lifecycle anaiero_storage_native anaiero_experiment_history)
  add_test(NAME anaiero_modular_session COMMAND ce_modular_session_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_BINARY_DIR}/modular-session-tests" engines)
  add_test(NAME anaiero_recorded_experiments COMMAND ce_modular_session_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_BINARY_DIR}/modular-session-tests" experiments)
  set_tests_properties(anaiero_modular_session anaiero_recorded_experiments PROPERTIES TIMEOUT 600)
endif()
