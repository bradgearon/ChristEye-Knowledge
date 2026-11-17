# The same authenticated-local-source simulation is rebuilt from disk twice.
if(NOT DEFINED CE_ANA OR NOT DEFINED CE_ATLAS OR NOT DEFINED CE_TEMP)
  message(FATAL_ERROR "Missing inputs for epoch smoke")
endif()
file(REMOVE_RECURSE "${CE_TEMP}")
function(run_epoch label)
  execute_process(COMMAND "${CE_ANA}" science epoch-demo "${CE_ATLAS}" "${CE_TEMP}"
    RESULT_VARIABLE rc OUTPUT_VARIABLE result ERROR_VARIABLE diagnostic)
  if(NOT "${rc}" STREQUAL "0" OR NOT result MATCHES "\"journal_events\":4" OR
     NOT result MATCHES "\"revision\":4" OR
     NOT result MATCHES "\"source_epochs\":3" OR
     NOT result MATCHES "\"stale\":2")
    message(FATAL_ERROR "${label}: ${rc}: ${result}: ${diagnostic}")
  endif()
endfunction()
run_epoch("first")
run_epoch("restart")
