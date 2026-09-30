# Explicit source cold initializer, using the single input/force-feedback owner.
# Include after cmake/source_input.cmake and the force-feedback state integration.
include_guard(GLOBAL)
foreach(required_target source_input_storage menu_input_producer race_session)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "cold input requires existing target ${required_target}")
  endif()
endforeach()
add_library(source_input_cold_init STATIC "${PROJECT_SOURCE_DIR}/src/source_input_cold_init.cpp")
target_link_libraries(source_input_cold_init PUBLIC source_input_storage menu_input_producer)
add_library(source_input_cold_init_session STATIC "${PROJECT_SOURCE_DIR}/src/source_input_cold_init_session.cpp")
target_link_libraries(source_input_cold_init_session PUBLIC race_session source_input_cold_init)
# Do not link either target back into source_input_storage: Session owns it.
# No automatic invocation is added to startup, scene transitions or draws.
if(BUILD_TESTING)
  add_executable(source_input_cold_init_tests "${PROJECT_SOURCE_DIR}/tests/source_input_cold_init_tests.cpp")
  target_link_libraries(source_input_cold_init_tests PRIVATE source_input_cold_init)
  add_test(NAME source_input_cold_init_receiving COMMAND source_input_cold_init_tests)
  set_tests_properties(source_input_cold_init_receiving PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
  add_executable(source_input_cold_session_tests "${PROJECT_SOURCE_DIR}/tests/source_input_cold_session_tests.cpp")
  target_link_libraries(source_input_cold_session_tests PRIVATE source_input_cold_init_session)
  add_test(NAME source_input_cold_session_receiving COMMAND source_input_cold_session_tests "${PROJECT_SOURCE_DIR}/data/retail")
  set_tests_properties(source_input_cold_session_receiving PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
endif()
