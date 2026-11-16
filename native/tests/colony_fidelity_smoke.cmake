# SIM3.2 end-to-end: committed colony replay -> frozen holdout -> JSON report.
if(NOT DEFINED CE_RUNNER OR NOT DEFINED CE_FIDELITY OR
   NOT DEFINED CE_REPO OR NOT DEFINED CE_TEMP)
  message(FATAL_ERROR "Required SIM3.2 CMake args missing")
endif()
file(REMOVE_RECURSE "${CE_TEMP}")
file(MAKE_DIRECTORY "${CE_TEMP}")
execute_process(COMMAND "${CE_RUNNER}" step
  "${CE_REPO}/examples/connected-gardens.cev" "${CE_TEMP}/run" 6
  RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "SIM3 replay failed: ${rc}\n${stdout}\n${stderr}")
endif()
execute_process(COMMAND "${CE_FIDELITY}" evaluate
  "${CE_REPO}/examples/connected-gardens.cev" "${CE_TEMP}/run"
  "${CE_REPO}/examples/sim3/baseline.ceveval" "${CE_REPO}"
  "${CE_TEMP}/model-report.json" "${CE_TEMP}/registry"
  RESULT_VARIABLE rc OUTPUT_VARIABLE stdout ERROR_VARIABLE stderr)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "SIM3.2 evaluation failed: ${rc}\n${stdout}\n${stderr}")
endif()
if(NOT EXISTS "${CE_TEMP}/model-report.json")
  message(FATAL_ERROR "SIM3.2 report missing")
endif()
file(READ "${CE_TEMP}/model-report.json" report)
if(NOT report MATCHES "SYNTHETIC_ONLY_NOT_BIOLOGICAL_VALIDATION" OR
   NOT report MATCHES "independent_holdout_verified" OR
   NOT report MATCHES "0[.]750000" OR
   NOT report MATCHES "1[.]500000")
  message(FATAL_ERROR "SIM3.2 report has unexpected content: ${report}")
endif()
execute_process(COMMAND "${CE_FIDELITY}" evaluate
  "${CE_REPO}/examples/connected-gardens.cev" "${CE_TEMP}/run"
  "${CE_REPO}/examples/sim3/baseline.ceveval" "${CE_REPO}"
  "${CE_TEMP}/model-report.json"
  RESULT_VARIABLE rc OUTPUT_QUIET ERROR_QUIET)
if(rc EQUAL 0)
  message(FATAL_ERROR "SIM3.2 unexpectedly overwrote an existing report")
endif()
message(STATUS "SIM3.2 deterministic CLI, report and WindStore smoke passed")
