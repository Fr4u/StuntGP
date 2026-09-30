# Exact v2 CFG link plus actual-platform receiving. Build wiring, no startup.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/source_boot_config.cmake")
if(BUILD_TESTING AND WIN32)
  add_executable(source_cold_config_platform_tests
    "${PROJECT_SOURCE_DIR}/tests/source_cold_config_platform_tests.cpp")
  target_link_libraries(source_cold_config_platform_tests PRIVATE source_boot_config_session)
  add_test(NAME source_cold_config_platform_contract COMMAND source_cold_config_platform_tests)
  add_executable(source_cold_config_platform_link_tests
    "${PROJECT_SOURCE_DIR}/tests/source_cold_config_platform_link_tests.cpp")
  # Deliberately no direct source_boot_config link: verifies required v2 edge.
  target_link_libraries(source_cold_config_platform_link_tests PRIVATE race_session)
  add_test(NAME source_cold_config_platform_transitive_link COMMAND source_cold_config_platform_link_tests)
endif()
