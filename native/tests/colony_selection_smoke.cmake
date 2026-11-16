# SIM3.3 true CLI integration: generate two independent simulation histories,
# choose by training only, then open the separately pinned test set.
foreach(required CE_RUNNER CE_SELECT CE_TEST CE_REPO CE_TEMP)
  if(NOT DEFINED ${required})
    message(FATAL_ERROR "Missing ${required}")
  endif()
endforeach()
file(REMOVE_RECURSE "${CE_TEMP}")
file(MAKE_DIRECTORY "${CE_TEMP}")
set(manifest "${CE_REPO}/examples/sim3/model-search.cevselect")
set(selected "${CE_TEMP}/winner.cevwinner")
set(report "${CE_TEMP}/final.json")
execute_process(
  COMMAND "${CE_SELECT}" prepare "${manifest}" "${CE_REPO}" "${CE_TEMP}/runs"
  RESULT_VARIABLE prepared OUTPUT_VARIABLE prepare_stdout ERROR_VARIABLE prepare_stderr)
if(NOT prepared EQUAL 0)
  message(FATAL_ERROR "SIM3.3 preparation failed: ${prepare_stdout} ${prepare_stderr}")
endif()
execute_process(
  COMMAND "${CE_SELECT}" select "${manifest}" "${CE_REPO}" "${CE_TEMP}/runs" "${selected}"
  RESULT_VARIABLE chosen OUTPUT_VARIABLE choose_stdout ERROR_VARIABLE choose_stderr)
if(NOT chosen EQUAL 0)
  message(FATAL_ERROR "SIM3.3 training-only selection failed: ${choose_stdout} ${choose_stderr}")
endif()
file(READ "${selected}" selected_bytes)
if(NOT selected_bytes MATCHES "winner baseline")
  message(FATAL_ERROR "Unexpected model winner: ${selected_bytes}")
endif()
execute_process(
  COMMAND "${CE_SELECT}" final "${manifest}" "${CE_REPO}" "${CE_TEMP}/runs" "${selected}"
          "${CE_REPO}/examples/sim3/model-search-test.cevsplits" "${report}"
  RESULT_VARIABLE tested OUTPUT_VARIABLE test_stdout ERROR_VARIABLE test_stderr)
if(NOT tested EQUAL 0)
  message(FATAL_ERROR "SIM3.3 independent test failed: ${test_stdout} ${test_stderr}")
endif()
file(READ "${report}" report_bytes)
if(NOT report_bytes MATCHES "SYNTHETIC_ONLY_NOT_BIOLOGICAL_VALIDATION" OR
   NOT report_bytes MATCHES "\"selection_locked\":true")
  message(FATAL_ERROR "SIM3.3 result missing provenance and selection-lock fields")
endif()
execute_process(
  COMMAND "${CE_TEST}" "${CE_REPO}" "${CE_TEMP}/runs"
  RESULT_VARIABLE unit OUTPUT_VARIABLE unit_stdout ERROR_VARIABLE unit_stderr)
if(NOT unit EQUAL 0)
  message(FATAL_ERROR "SIM3.3 negative regression tests failed: ${unit_stdout} ${unit_stderr}")
endif()
# Outputs must not be silently overwritten. Repeated final test must fail closed.
execute_process(
  COMMAND "${CE_SELECT}" final "${manifest}" "${CE_REPO}" "${CE_TEMP}/runs" "${selected}"
          "${CE_REPO}/examples/sim3/model-search-test.cevsplits" "${report}"
  RESULT_VARIABLE duplicate)
if(duplicate EQUAL 0)
  message(FATAL_ERROR "SIM3.3 overwrote the immutable final test report")
endif()
message(STATUS "SIM3.3 end-to-end training/test split and CLI integration passed")
