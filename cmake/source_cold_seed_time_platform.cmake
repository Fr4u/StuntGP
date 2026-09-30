# Build wiring only. No cold wrapper or automatic source invocation.
include_guard(GLOBAL)
if(NOT TARGET race_session)
  message(FATAL_ERROR "source seed/time platform binding requires race_session")
endif()
include("${CMAKE_CURRENT_LIST_DIR}/source_random_seed.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/source_cold_time.cmake")
if(BUILD_TESTING AND WIN32)
  add_executable(source_cold_seed_time_platform_tests
    "${PROJECT_SOURCE_DIR}/tests/source_cold_seed_time_platform_tests.cpp")
  target_link_libraries(source_cold_seed_time_platform_tests PRIVATE
    source_random_seed_session source_cold_time_session source_cold_time_win32)
  add_test(NAME source_cold_seed_time_platform_contract COMMAND source_cold_seed_time_platform_tests
    "${PROJECT_SOURCE_DIR}/evidence/retail/StuntGP_D3D.exe")
  # Explicit admitted receiving fixture, not a discovered CRT environment.
  add_test(NAME source_cold_seed_time_platform_native COMMAND source_cold_seed_time_platform_tests
    "${PROJECT_SOURCE_DIR}/evidence/retail/StuntGP_D3D.exe" --native-tz UTC0)
endif()
