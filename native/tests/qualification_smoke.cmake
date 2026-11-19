if(NOT DEFINED ANAIERO_QUALIFY OR NOT DEFINED ANAIERO_PROFILE OR NOT DEFINED ANAIERO_OUTPUT OR
   NOT DEFINED ANAIERO_BUILD_DIR OR NOT DEFINED ANAIERO_LAUNCHER)
  message(FATAL_ERROR "Qualification executable, profile and isolated output are required")
endif()
file(REMOVE_RECURSE "${ANAIERO_OUTPUT}")
execute_process(COMMAND "${ANAIERO_QUALIFY}" run "${ANAIERO_PROFILE}" "${ANAIERO_OUTPUT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 150)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "Qualification profile failed: ${result}\n${error}\n${output}")
endif()
file(READ "${ANAIERO_OUTPUT}/report.json" report)
string(JSON schema GET "${report}" schema)
string(JSON passed GET "${report}" passed)
string(JSON observations GET "${report}" workload observations)
string(JSON epochs GET "${report}" workload epoch_transitions)
string(JSON active GET "${report}" workload maximum_active_sources)
string(JSON audited GET "${report}" audit passed)
string(JSON restarted GET "${report}" audit restart_identity_verified)
string(JSON identity GET "${report}" profile_sha256)
string(JSON percentile_method GET "${report}" percentile_method)
file(SHA256 "${ANAIERO_PROFILE}" expected_identity)
if(NOT schema STREQUAL "anaiero.native-qualification/1" OR NOT passed OR NOT audited OR
   NOT restarted OR NOT observations EQUAL 256 OR NOT epochs EQUAL 4 OR NOT active EQUAL 1 OR
   NOT identity STREQUAL expected_identity OR
   NOT percentile_method STREQUAL "log2-ceiling-capped-by-observed-maximum/1")
  message(FATAL_ERROR "Qualification report did not establish its declared workload: ${report}")
endif()
string(JSON maximum_ns GET "${report}" commit maximum_ns)
foreach(percentile p50_upper_ns p95_upper_ns p99_upper_ns)
  string(JSON upper_ns GET "${report}" commit "${percentile}")
  if(upper_ns GREATER maximum_ns)
    message(FATAL_ERROR "Reported commit ${percentile} exceeds its exact observed maximum")
  endif()
endforeach()
execute_process(COMMAND "${ANAIERO_QUALIFY}" run "${ANAIERO_PROFILE}" "${ANAIERO_OUTPUT}"
  RESULT_VARIABLE repeated OUTPUT_QUIET ERROR_QUIET)
file(READ "${ANAIERO_OUTPUT}/report.json" preserved)
if(repeated EQUAL 0 OR NOT preserved STREQUAL report)
  message(FATAL_ERROR "Qualification rerun replaced an existing report")
endif()
file(READ "${ANAIERO_PROFILE}" profile)
file(WRITE "${ANAIERO_OUTPUT}/invalid-profile" "${profile}\nmaximum_commit_p99_us=1\n")
execute_process(COMMAND "${ANAIERO_QUALIFY}" run "${ANAIERO_OUTPUT}/invalid-profile" "${ANAIERO_OUTPUT}/invalid-run"
  RESULT_VARIABLE invalid OUTPUT_QUIET ERROR_QUIET)
if(invalid EQUAL 0 OR EXISTS "${ANAIERO_OUTPUT}/invalid-run")
  message(FATAL_ERROR "Duplicate profile budget was accepted or mutated output")
endif()
configure_file("${ANAIERO_PROFILE}" "${ANAIERO_OUTPUT}/profile with.space.cevqualification" COPYONLY)
execute_process(COMMAND "${CMAKE_COMMAND}"
  "-DANAIERO_BUILD_DIR=${ANAIERO_BUILD_DIR}"
  "-DANAIERO_PROFILE=profile with.space.cevqualification"
  "-DANAIERO_OUTPUT=launched with.space" "-DANAIERO_CONFIG=${ANAIERO_CONFIG}"
  -P "${ANAIERO_LAUNCHER}" WORKING_DIRECTORY "${ANAIERO_OUTPUT}"
  RESULT_VARIABLE launched OUTPUT_VARIABLE launched_output ERROR_VARIABLE launched_error TIMEOUT 150)
if(NOT launched EQUAL 0)
  message(FATAL_ERROR "Qualification launcher lost its exact relative/space/dot paths: ${launched_output}\n${launched_error}")
endif()
file(READ "${ANAIERO_OUTPUT}/launched with.space/report.json" launched_report)
string(JSON launched_passed GET "${launched_report}" passed)
string(JSON launched_identity GET "${launched_report}" profile_sha256)
if(NOT launched_passed OR NOT launched_identity STREQUAL expected_identity)
  message(FATAL_ERROR "Qualification launcher used a different profile or failed its workload")
endif()
message(STATUS "Qualification report and immutable profile verified: ${ANAIERO_OUTPUT}/report.json")
