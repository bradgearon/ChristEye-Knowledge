# Python sidecar is optional for actual users; CI verifies the stdio exchange.
if(NOT DEFINED CE_ANA OR NOT DEFINED CE_PYTHON OR NOT DEFINED CE_DRIVER OR
   NOT DEFINED CE_ATLAS OR NOT DEFINED CE_TEMP)
  message(FATAL_ERROR "Missing live solver smoke arguments")
endif()
file(REMOVE_RECURSE "${CE_TEMP}")
function(run_live label epoch expected_total)
  execute_process(COMMAND "${CE_PYTHON}" "${CE_DRIVER}"
     --anaiero "${CE_ANA}" --atlas "${CE_ATLAS}" --store "${CE_TEMP}/store"
     --epoch "${epoch}" --backend synthetic
     RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic)
  if(NOT "${rc}" STREQUAL "0" OR
     NOT output MATCHES "\"observations\":112" OR
     NOT output MATCHES "\"macro_steps\":28" OR
     NOT output MATCHES "\"windstore_events\":${expected_total}")
    message(FATAL_ERROR "${label}: ${rc} ${output} ${diagnostic}")
  endif()
endfunction()
run_live("first" 1 112)
run_live("new-epoch" 2 224)
# Wrong source sequence must fail closed, not report a completed run.
execute_process(COMMAND "${CE_PYTHON}" "${CE_DRIVER}"
  --anaiero "${CE_ANA}" --atlas "${CE_ATLAS}"
  --store "${CE_TEMP}/failure" --epoch 3 --backend synthetic
  --invalid-at 2 --expect-failure
  RESULT_VARIABLE rc OUTPUT_VARIABLE output ERROR_VARIABLE diagnostic)
if(NOT "${rc}" STREQUAL "0" OR NOT output MATCHES "expected_failure")
  message(FATAL_ERROR "bad remote sequence was not rejected: ${rc} ${output} ${diagnostic}")
endif()
