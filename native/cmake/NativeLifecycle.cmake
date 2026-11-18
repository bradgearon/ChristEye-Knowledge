add_library(anaiero_lifecycle STATIC src/lifecycle/lifecycle.cpp)
target_link_libraries(anaiero_lifecycle PUBLIC anaiero_contracts anaiero_evidence)
anaiero_sdk_target(anaiero_lifecycle)
# The domain library only sees installed SDK headers. Native archive files,
# writer locks and durable replacement belong to the existing native adapter.
target_sources(anaiero_storage_native PRIVATE src/lifecycle/native_archive.cpp src/lifecycle/native_restore.cpp)
target_link_libraries(anaiero_storage_native PRIVATE anaiero_lifecycle)
if(WIN32)
  target_link_libraries(anaiero_storage_native PRIVATE advapi32)
endif()
install(TARGETS anaiero_lifecycle EXPORT AnaieroTargets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
if(CE_WIND_BUILD_TESTS)
  add_executable(anaiero_lifecycle_tests tests/lifecycle_tests.cpp)
  target_link_libraries(anaiero_lifecycle_tests PRIVATE anaiero_lifecycle anaiero_storage_native)
  if(MSVC)
    target_compile_options(anaiero_lifecycle_tests PRIVATE /W4 /WX)
  else()
    target_compile_options(anaiero_lifecycle_tests PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
  add_test(NAME anaiero_lifecycle_tests COMMAND anaiero_lifecycle_tests
    ${CMAKE_CURRENT_BINARY_DIR}/lifecycle-test-store)
endif()
