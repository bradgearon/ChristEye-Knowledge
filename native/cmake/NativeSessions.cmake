# Reusable experiments, sustained stores and supervised scientific engines.
function(anaiero_native_target target source)
  add_executable(${target} ${source})
  target_link_libraries(${target} PRIVATE anaiero_application Threads::Threads)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /WX)
  else()
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
endfunction()

anaiero_native_target(ce_scientific_solver_provider examples/scientific_solver_provider.cpp)
anaiero_native_target(ce_scientific_config examples/scientific_configuration.cpp)
install(TARGETS ce_scientific_solver_provider ce_scientific_config RUNTIME DESTINATION bin)
if(TARGET anaiero)
  add_dependencies(anaiero ce_scientific_solver_provider ce_scientific_config)
endif()

if(CE_WIND_BUILD_TESTS)
  foreach(part experiment operation segmented_storage supervised_science session_batch)
    anaiero_native_target(ce_${part}_tests tests/${part}_tests.cpp)
  endforeach()
  target_compile_definitions(ce_experiment_tests PRIVATE CE_DURABLE_FAULT_TESTS=1)
  target_compile_definitions(ce_segmented_storage_tests PRIVATE CE_DURABLE_FAULT_TESTS=1)
  target_compile_definitions(ce_session_batch_tests PRIVATE CE_DURABLE_FAULT_TESTS=1)
  add_test(NAME anaiero_experiment_runtime COMMAND ce_experiment_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/cells/crest-blood-neural.cevatlas
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/connected-gardens.cev
    ${CMAKE_CURRENT_BINARY_DIR}/experiment-tests)
  add_test(NAME anaiero_operation_recovery COMMAND ce_operation_tests ${CMAKE_CURRENT_BINARY_DIR}/operation-tests)
  add_test(NAME anaiero_segmented_storage COMMAND ce_segmented_storage_tests ${CMAKE_CURRENT_BINARY_DIR}/segmented-tests)
  add_test(NAME anaiero_supervised_science COMMAND ce_supervised_science_tests
    $<TARGET_FILE:ce_scientific_solver_provider>
    ${CMAKE_CURRENT_SOURCE_DIR}/../examples/scientific/passive-membrane.cevmodel
    ${CMAKE_CURRENT_BINARY_DIR}/supervised-science-tests)
  add_test(NAME anaiero_session_batch COMMAND ce_session_batch_tests
    ${CMAKE_CURRENT_SOURCE_DIR}/.. $<TARGET_FILE:ce_scientific_solver_provider>
    ${CMAKE_CURRENT_BINARY_DIR}/session-batch-tests)
  set_tests_properties(anaiero_experiment_runtime anaiero_operation_recovery anaiero_segmented_storage
    anaiero_supervised_science anaiero_session_batch PROPERTIES TIMEOUT 300)

  set(ANAIERO_NEURON_PYTHON "" CACHE FILEPATH "Optional pinned NEURON Python used only for solver qualification")
  if(ANAIERO_NEURON_PYTHON)
    anaiero_native_target(ce_neuron_qualification_tests tests/neuron_qualification_tests.cpp)
    add_test(NAME anaiero_neuron_qualification COMMAND ce_neuron_qualification_tests
      ${ANAIERO_NEURON_PYTHON}
      ${CMAKE_CURRENT_SOURCE_DIR}/../tools/scientific/supervised_neuron_provider.py
      ${CMAKE_CURRENT_SOURCE_DIR}/../examples/scientific/passive-membrane.cevmodel
      ${CMAKE_CURRENT_BINARY_DIR}/neuron-qualification)
    set_tests_properties(anaiero_neuron_qualification PROPERTIES TIMEOUT 120)
  endif()
endif()
