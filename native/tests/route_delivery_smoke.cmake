# CLI integration across two independent processes.
if(NOT DEFINED CE_CLI OR NOT DEFINED CE_ATLAS OR NOT DEFINED CE_TEMP)
  message(FATAL_ERROR "Missing CLI inputs")
endif()
file(REMOVE_RECURSE "${CE_TEMP}")
execute_process(COMMAND "${CE_CLI}" fork demo "${CE_ATLAS}" "${CE_TEMP}/route"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "0" OR NOT out MATCHES "SW5_BRANCHES")
  message(FATAL_ERROR "demo failed: ${rc} ${out} ${err}")
endif()
execute_process(COMMAND "${CE_CLI}" fork sync "${CE_ATLAS}" "${CE_TEMP}/route"
  "${CE_TEMP}/wind" test-session 31 RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "0" OR NOT out MATCHES "\"acked\":true" OR
   NOT out MATCHES "\"journal_events\":1" OR NOT out MATCHES "\"projector_revision\":1")
  message(FATAL_ERROR "first sync: ${rc} ${out} ${err}")
endif()
execute_process(COMMAND "${CE_CLI}" fork sync "${CE_ATLAS}" "${CE_TEMP}/route"
  "${CE_TEMP}/wind" test-session 31 RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "0" OR NOT out MATCHES "\"acked\":true" OR
   NOT out MATCHES "\"journal_events\":1" OR NOT out MATCHES "\"projector_revision\":1")
  message(FATAL_ERROR "restart sync: ${rc} ${out} ${err}")
endif()
