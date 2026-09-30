# Private candidate. Main integrates after the existing race_session target.
# No source parent, clock acquisition, init/cadence hook or world member added.
include_guard(GLOBAL)
add_library(source_random_seed STATIC "${PROJECT_SOURCE_DIR}/src/source_random_seed.cpp")
target_link_libraries(source_random_seed PUBLIC project_options)
add_library(source_random_seed_session STATIC "${PROJECT_SOURCE_DIR}/src/source_random_seed_session.cpp")
target_link_libraries(source_random_seed_session PUBLIC source_random_seed race_session)
if(BUILD_TESTING)
  add_executable(source_random_seed_tests "${PROJECT_SOURCE_DIR}/tests/source_random_seed_tests.cpp")
  target_link_libraries(source_random_seed_tests PRIVATE source_random_seed)
  add_test(NAME source_random_seed_receiving COMMAND source_random_seed_tests)
endif()
