# Existing language core plus real persistent-platform receiving; no startup call.
include_guard(GLOBAL)
include("${CMAKE_CURRENT_LIST_DIR}/source_cold_language.cmake")
if(BUILD_TESTING AND TARGET source_cold_language_session)
  add_executable(source_cold_language_platform_tests
    "${PROJECT_SOURCE_DIR}/tests/source_cold_language_platform_tests.cpp")
  target_link_libraries(source_cold_language_platform_tests PRIVATE source_cold_language_session)
  add_test(NAME source_cold_language_platform_contract COMMAND source_cold_language_platform_tests
    --fixtures "${CMAKE_CURRENT_BINARY_DIR}/source-cold-language-platform-fixtures")
  add_test(NAME source_cold_language_platform_retail COMMAND source_cold_language_platform_tests
    --retail "${PROJECT_SOURCE_DIR}/data/retail")
  set_tests_properties(source_cold_language_platform_contract source_cold_language_platform_retail
    PROPERTIES RUN_SERIAL TRUE TIMEOUT 60)
endif()
