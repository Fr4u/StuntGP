# Receiving only. Main includes after source_outer_scheduler/source_input_cold_init
# and after the sealed construction-only platform/attachment dependency is imported.
include_guard(GLOBAL)
foreach(required_target project_options session_outer_capture source_input_cold_init_session)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "cold clock/input receiving requires actual target ${required_target}")
  endif()
endforeach()
if(BUILD_TESTING)
  add_executable(source_cold_clock_input_platform_receiving
    "${PROJECT_SOURCE_DIR}/tests/source_cold_clock_input_platform_receiving.cpp")
  target_link_libraries(source_cold_clock_input_platform_receiving PRIVATE
    project_options session_outer_capture source_input_cold_init_session)
  add_test(NAME source_cold_clock_input_platform_receiving
    COMMAND source_cold_clock_input_platform_receiving "${PROJECT_SOURCE_DIR}/data/retail")
  set_tests_properties(source_cold_clock_input_platform_receiving PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
endif()
