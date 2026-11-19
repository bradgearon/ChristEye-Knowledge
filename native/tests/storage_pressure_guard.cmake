if(NOT DEFINED ANAIERO_PRESSURE OR NOT DEFINED ANAIERO_GUARD_ROOT)
  message(FATAL_ERROR "Pressure test executable and a dedicated guard fixture root are required")
endif()
file(MAKE_DIRECTORY "${ANAIERO_GUARD_ROOT}")
string(RANDOM LENGTH 20 ALPHABET 0123456789abcdef nonce)
set(root "${ANAIERO_GUARD_ROOT}/${nonce}")
file(MAKE_DIRECTORY "${root}")
file(WRITE "${root}/OWNER" "ANAIERO-STORAGE-PRESSURE/1\nnonce=${nonce}\n")
# Even with a matching marker, a normal workspace is never a disposable tiny
# volume. No filler, store or report is allowed to be created on refusal.
execute_process(COMMAND "${ANAIERO_PRESSURE}" "${root}" "${nonce}" "${ANAIERO_GUARD_ROOT}/report-${nonce}"
  RESULT_VARIABLE result OUTPUT_QUIET ERROR_VARIABLE detail)
if(result EQUAL 0 OR NOT detail MATCHES "refusing to fill|distinct mount|isolated volume root|different volumes|isolated tmpfs" OR
   EXISTS "${root}/owned-pressure-fill.bin" OR EXISTS "${root}/store" OR EXISTS "${ANAIERO_GUARD_ROOT}/report-${nonce}")
  message(FATAL_ERROR "Pressure fixture failed safe workspace refusal: ${result}: ${detail}")
endif()
file(REMOVE "${root}/OWNER")
file(REMOVE_RECURSE "${root}")
message(STATUS "Ordinary workspace rejected before writes; this guard is not filesystem exhaustion evidence")
