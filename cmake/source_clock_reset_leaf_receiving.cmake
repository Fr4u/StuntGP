# Explicit CPU receiving for the 442100 leaf. Does not attach scene callers.
if(NOT BUILD_TESTING OR NOT TARGET session_outer_capture)
  return()
endif()
include_guard(GLOBAL)
if(BUILD_TESTING AND TARGET session_outer_capture)
  add_executable(source_clock_reset_leaf_receiving
    "${PROJECT_SOURCE_DIR}/tests/source_clock_reset_leaf_receiving.cpp")
  target_link_libraries(source_clock_reset_leaf_receiving PRIVATE project_options session_outer_capture)
  add_executable(source_clock_reset_frontend_regression
    "${PROJECT_SOURCE_DIR}/tests/retail_frontend_clock_oracle.cpp")
  target_link_libraries(source_clock_reset_frontend_regression PRIVATE project_options retail_frontend_clock)
  set(SGN_CLOCK_RESET_RETAIL "${PROJECT_SOURCE_DIR}/data/retail" CACHE PATH "NEW retail root for construction-only platform")
  set(SGN_CLOCK_RESET_CORPUS "${PROJECT_SOURCE_DIR}/reports/vehicle-work/series6/frontend-clock-v1/clock.dat" CACHE FILEPATH "Immutable SGNFCLK1 corpus")
  add_test(NAME source_clock_reset_leaf_cpu COMMAND source_clock_reset_leaf_receiving "${SGN_CLOCK_RESET_RETAIL}")
  add_test(NAME source_clock_reset_frontend_immutable COMMAND source_clock_reset_frontend_regression "${SGN_CLOCK_RESET_CORPUS}")
  set_tests_properties(source_clock_reset_leaf_cpu source_clock_reset_frontend_immutable PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
endif()
