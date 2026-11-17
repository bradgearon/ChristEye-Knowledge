if(NOT DEFINED CE_KERNEL OR NOT DEFINED CE_ATLAS OR NOT DEFINED CE_SIM3 OR NOT DEFINED CE_TEMP)
  message(FATAL_ERROR "Missing kernel smoke inputs")
endif()
file(REMOVE_RECURSE "${CE_TEMP}")
execute_process(COMMAND "${CE_KERNEL}" demo "${CE_ATLAS}" "${CE_SIM3}" "${CE_TEMP}"
  RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
if(NOT "${rc}" STREQUAL "0" OR NOT out MATCHES "\"sw5_tick\":4" OR
   NOT out MATCHES "\"sim3_tick\":2" OR
   NOT out MATCHES "\"published_snapshots\":6" OR
   NOT out MATCHES "\"sim3_durable\":true")
  message(FATAL_ERROR "simulation kernel failed: ${rc} ${out} ${err}")
endif()
