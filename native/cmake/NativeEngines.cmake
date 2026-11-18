# Each built-in owns its model dependencies privately; portable clients inherit
# only the installed SDK. The source/build manifest prevents silent interpreter
# upgrades when a retained checkpoint pins a compiled implementation.
file(GLOB_RECURSE anaiero_builtin_inputs CONFIGURE_DEPENDS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/modular_session_cli.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/modular_host_cli.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/modular_mesh_cli.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/examples/native_qualification_cli.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include/*.hpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/sdk/include/*.h"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/*.cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt"
  "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${anaiero_builtin_inputs})
set(anaiero_builtin_manifest "Anaiero builtins/1\n${CMAKE_CXX_COMPILER_ID}\n${CMAKE_CXX_COMPILER_VERSION}\n${CMAKE_SYSTEM_NAME}\n${CMAKE_SYSTEM_PROCESSOR}\n${CMAKE_BUILD_TYPE}\n${CMAKE_CXX_FLAGS}\n${CMAKE_CXX_FLAGS_RELEASE}\n${CMAKE_CXX_FLAGS_DEBUG}\n${CMAKE_CXX_FLAGS_RELWITHDEBINFO}\n${CMAKE_CXX_FLAGS_MINSIZEREL}\n")
foreach(source IN LISTS anaiero_builtin_inputs)
  file(SHA256 "${source}" digest)
  file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}" "${source}")
  string(APPEND anaiero_builtin_manifest "${relative} ${digest}\n")
endforeach()
if(CMAKE_CONFIGURATION_TYPES)
  set(anaiero_builtin_definitions)
  foreach(profile IN LISTS CMAKE_CONFIGURATION_TYPES)
    set(manifest "${anaiero_builtin_manifest}Configuration: ${profile}\nCMake: ${CMAKE_VERSION}\nMSVC runtime: ${CMAKE_MSVC_RUNTIME_LIBRARY}\n")
    string(SHA256 digest "${manifest}")
    list(APPEND anaiero_builtin_definitions "$<$<CONFIG:${profile}>:ANAIERO_BUILTIN_BUILD_ID=\"${digest}\">")
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/anaiero-builtin-manifest-${profile}.txt" "${manifest}")
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/anaiero-builtin-manifest-${profile}.txt"
      DESTINATION share/anaiero RENAME anaiero-builtin-manifest.txt CONFIGURATIONS "${profile}")
  endforeach()
else()
  string(APPEND anaiero_builtin_manifest "CMake: ${CMAKE_VERSION}\nMSVC runtime: ${CMAKE_MSVC_RUNTIME_LIBRARY}\n")
  string(SHA256 anaiero_builtin_digest "${anaiero_builtin_manifest}")
  set(anaiero_builtin_definitions "ANAIERO_BUILTIN_BUILD_ID=\"${anaiero_builtin_digest}\"")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/anaiero-builtin-manifest.txt" "${anaiero_builtin_manifest}")
  install(FILES "${CMAKE_CURRENT_BINARY_DIR}/anaiero-builtin-manifest.txt" DESTINATION share/anaiero)
endif()
foreach(engine cell colony plan numerical)
  if(engine STREQUAL "numerical")
    set(source src/engines/numerical_modules.cpp)
  else()
    set(source "src/engines/${engine}_module.cpp")
  endif()
  add_library(anaiero_engine_${engine} STATIC ${source})
  anaiero_sdk_target(anaiero_engine_${engine})
  target_link_libraries(anaiero_engine_${engine} PUBLIC anaiero_catalog)
  target_include_directories(anaiero_engine_${engine} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_compile_definitions(anaiero_engine_${engine} PRIVATE ${anaiero_builtin_definitions})
  if(MSVC)
    target_compile_options(anaiero_engine_${engine} PRIVATE /W4 /WX)
  else()
    target_compile_options(anaiero_engine_${engine} PRIVATE -Wall -Wextra -Wconversion -Werror)
  endif()
endforeach()
add_library(anaiero_engines STATIC src/engines/registration.cpp)
anaiero_sdk_target(anaiero_engines)
target_link_libraries(anaiero_engines PUBLIC anaiero_catalog PRIVATE
  anaiero_engine_cell anaiero_engine_colony anaiero_engine_plan anaiero_engine_numerical)
target_compile_definitions(anaiero_engines PRIVATE ${anaiero_builtin_definitions})
install(TARGETS anaiero_engines anaiero_engine_cell anaiero_engine_colony anaiero_engine_plan anaiero_engine_numerical
  EXPORT AnaieroTargets ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)

add_library(anaiero_session_runner STATIC src/session/session_runner.cpp)
anaiero_sdk_target(anaiero_session_runner)
target_link_libraries(anaiero_session_runner PUBLIC anaiero_runtime anaiero_evidence)
add_library(anaiero_experiment_history STATIC src/session/experiment_history.cpp)
anaiero_sdk_target(anaiero_experiment_history)
target_link_libraries(anaiero_experiment_history PUBLIC anaiero_runtime anaiero_evidence anaiero_experiments)
add_library(anaiero_engine_files STATIC src/engine_adapters/native_configurations.cpp src/engine_adapters/native_artifact.cpp)
anaiero_sdk_target(anaiero_engine_files)
target_link_libraries(anaiero_engine_files PUBLIC anaiero_engines anaiero_session_runner)
target_include_directories(anaiero_engine_files PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
include(${CMAKE_CURRENT_LIST_DIR}/NativeArtifactTests.cmake)
install(TARGETS anaiero_session_runner anaiero_engine_files anaiero_experiment_history EXPORT AnaieroTargets
  ARCHIVE DESTINATION lib LIBRARY DESTINATION lib RUNTIME DESTINATION bin)
if(CE_WIND_BUILD_TESTS)
  enable_testing()
  add_executable(ce_modular_engine_tests tests/modular_engine_tests.cpp)
  anaiero_sdk_target(ce_modular_engine_tests)
  target_include_directories(ce_modular_engine_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
  target_link_libraries(ce_modular_engine_tests PRIVATE anaiero_engines anaiero_experiments)
  add_test(NAME anaiero_modular_engines COMMAND ce_modular_engine_tests
    "${CMAKE_CURRENT_SOURCE_DIR}/.." "${CMAKE_CURRENT_BINARY_DIR}/modular-engine-tests")
  include(${CMAKE_CURRENT_LIST_DIR}/ModularSessionTests.cmake)
endif()

add_executable(ce_modular_session examples/modular_session_cli.cpp)
anaiero_sdk_target(ce_modular_session)
target_link_libraries(ce_modular_session PRIVATE anaiero_engine_files anaiero_session_runner
  anaiero_lifecycle anaiero_storage_native anaiero_experiments anaiero_experiment_history Threads::Threads)
install(TARGETS ce_modular_session RUNTIME DESTINATION bin)
if(CE_WIND_BUILD_TESTS)
  add_test(NAME anaiero_modular_cli COMMAND ${CMAKE_COMMAND}
    -DCE_SESSION=$<TARGET_FILE:ce_modular_session>
    -DCE_REPO=${CMAKE_CURRENT_SOURCE_DIR}/..
    -DCE_TEMP=${CMAKE_CURRENT_BINARY_DIR}/modular-cli-tests
    -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/modular_cli_smoke.cmake)
  set_tests_properties(anaiero_modular_cli PROPERTIES TIMEOUT 600)
endif()
function(anaiero_attach_modular_cli)
  if(TARGET anaiero)
    add_dependencies(anaiero ce_modular_session)
  endif()
endfunction()
cmake_language(DEFER CALL anaiero_attach_modular_cli)
