# Native canonical ledger and sealed ordered index. No database/Python runtime
# or third-party download is required by the Windows/Linux controller profile.
add_library(anaiero_evidence STATIC src/evidence/evidence.cpp src/evidence/migration_receipts.cpp src/evidence/migration_evidence_service.cpp)
anaiero_sdk_target(anaiero_evidence)
target_link_libraries(anaiero_evidence PUBLIC anaiero_contracts PRIVATE Threads::Threads)
add_library(anaiero_storage_native STATIC src/storage/sealed_index.cpp src/storage/packed_index.cpp src/storage/native_store.cpp src/storage/legacy_migration.cpp)
anaiero_sdk_target(anaiero_storage_native)
target_include_directories(anaiero_storage_native PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(anaiero_storage_native PUBLIC anaiero_evidence)
install(TARGETS anaiero_evidence anaiero_storage_native EXPORT AnaieroTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(CE_WIND_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/canonical_evidence_tests.cpp)
  add_executable(anaiero_evidence_tests tests/canonical_evidence_tests.cpp)
  anaiero_sdk_target(anaiero_evidence_tests)
  target_link_libraries(anaiero_evidence_tests PRIVATE anaiero_storage_native Threads::Threads)
  add_test(NAME anaiero_evidence_tests COMMAND anaiero_evidence_tests)
endif()
if(CE_WIND_BUILD_TESTS AND EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tests/migration_tests.cpp)
  add_executable(anaiero_migration_tests tests/migration_tests.cpp)
  anaiero_sdk_target(anaiero_migration_tests)
  target_include_directories(anaiero_migration_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
  target_link_libraries(anaiero_migration_tests PRIVATE anaiero_storage_native)
  add_test(NAME anaiero_migration_tests COMMAND anaiero_migration_tests)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(anaiero_compaction_tests tests/canonical_compaction_tests.cpp)
  anaiero_sdk_target(anaiero_compaction_tests)
  target_link_libraries(anaiero_compaction_tests PRIVATE anaiero_storage_native)
  add_test(NAME anaiero_compaction_tests COMMAND anaiero_compaction_tests)
endif()
if(CE_WIND_BUILD_TESTS)
  add_executable(anaiero_commit_timing_tests tests/commit_timing_tests.cpp)
  anaiero_sdk_target(anaiero_commit_timing_tests)
  target_link_libraries(anaiero_commit_timing_tests PRIVATE anaiero_storage_native)
  add_test(NAME anaiero_commit_timings COMMAND anaiero_commit_timing_tests)
  add_executable(anaiero_index_staging_tests tests/index_staging_tests.cpp)
  anaiero_sdk_target(anaiero_index_staging_tests)
  target_include_directories(anaiero_index_staging_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/storage)
  target_link_libraries(anaiero_index_staging_tests PRIVATE anaiero_storage_native)
  add_test(NAME anaiero_index_staging_tests COMMAND anaiero_index_staging_tests)
endif()
if(CE_WIND_BUILD_TESTS)
  set(old_index_fixture "${CMAKE_CURRENT_BINARY_DIR}/packed-index-compatibility-input")
  file(ARCHIVE_EXTRACT
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/pre-packed-index-9e6a593.zip"
    DESTINATION "${old_index_fixture}")
  add_executable(anaiero_packed_recovery_tests tests/packed_index_recovery_tests.cpp)
  anaiero_sdk_target(anaiero_packed_recovery_tests)
  target_link_libraries(anaiero_packed_recovery_tests PRIVATE anaiero_storage_native)
  add_test(NAME anaiero_packed_index_recovery
    COMMAND anaiero_packed_recovery_tests "${old_index_fixture}")
  add_test(NAME anaiero_packed_index_process_recovery COMMAND ${CMAKE_COMMAND}
    "-DTEST_EXECUTABLE=$<TARGET_FILE:anaiero_packed_recovery_tests>"
    "-DFIXTURE_SOURCE=${old_index_fixture}"
    "-DBUILD_DIRECTORY=${CMAKE_CURRENT_BINARY_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/packed_index_process_cut.cmake")
  set_tests_properties(anaiero_packed_index_recovery anaiero_packed_index_process_recovery PROPERTIES
    TIMEOUT 300 LABELS "recovery;storage")
endif()
