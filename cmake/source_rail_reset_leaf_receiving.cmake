# CPU qualification of the 4192D0 leaf; no native source or backend execution.
if(NOT BUILD_TESTING OR NOT TARGET race_session)
  return()
endif()
include_guard(GLOBAL)
add_executable(source_rail_reset_leaf_receiving
  "${PROJECT_SOURCE_DIR}/tests/source_rail_reset_leaf_receiving.cpp")
target_link_libraries(source_rail_reset_leaf_receiving PRIVATE project_options race_session)
add_executable(source_rail_reset_rail_regression
  "${PROJECT_SOURCE_DIR}/tests/contact_rail_oracle.cpp")
target_link_libraries(source_rail_reset_rail_regression PRIVATE project_options contact_rail)
add_executable(source_rail_reset_full_tick_regression
  "${PROJECT_SOURCE_DIR}/tests/vehicle_tick_full_oracle.cpp")
target_link_libraries(source_rail_reset_full_tick_regression PRIVATE project_options race_session)
set(SGN_RAIL_RESET_RETAIL "${PROJECT_SOURCE_DIR}/data/retail" CACHE PATH "NEW retail data")
set(SGN_RAIL_RESET_FIXTURE "" CACHE FILEPATH "Pinned first-state extraction for authored reset receiving")
set(SGN_RAIL_RESET_RAIL_CORPUS "${PROJECT_SOURCE_DIR}/reports/contact-work/series2/rail-final/rail.dat" CACHE FILEPATH "Immutable rail corpus")
set(SGN_RAIL_RESET_TICK_CORPUS "${PROJECT_SOURCE_DIR}/reports/vehicle-work/series2/full-tick-source-v7/full-tick.dat" CACHE FILEPATH "Immutable whole-tick corpus")
if(SGN_RAIL_RESET_FIXTURE)
  add_test(NAME source_rail_reset_leaf_cpu COMMAND source_rail_reset_leaf_receiving
    "${SGN_RAIL_RESET_RETAIL}" "${SGN_RAIL_RESET_FIXTURE}")
  set_tests_properties(source_rail_reset_leaf_cpu PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
endif()
add_test(NAME source_rail_reset_rail_immutable COMMAND source_rail_reset_rail_regression
  "${SGN_RAIL_RESET_RAIL_CORPUS}" "${SGN_RAIL_RESET_RETAIL}/meshdata/track1.PMD")
add_test(NAME source_rail_reset_full_tick_immutable COMMAND source_rail_reset_full_tick_regression
  "${SGN_RAIL_RESET_TICK_CORPUS}" "${SGN_RAIL_RESET_RETAIL}")
set_tests_properties(source_rail_reset_rail_immutable source_rail_reset_full_tick_immutable
  PROPERTIES TIMEOUT 300 RUN_SERIAL TRUE)
