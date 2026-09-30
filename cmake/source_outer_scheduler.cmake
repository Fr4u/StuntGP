# Include after source_input and session_effect_resources, inside WIN32 runtime.
include_guard(GLOBAL)
foreach(required_target session_outer_capture race_session_iteration source_input_session session_window_input retail_scene_transition session_effect_resources)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "source outer requires actual target ${required_target}")
  endif()
endforeach()
if(NOT TARGET retail_frontend_clock)
  add_library(retail_frontend_clock STATIC "${PROJECT_SOURCE_DIR}/src/retail_frontend_clock.cpp")
  target_link_libraries(retail_frontend_clock PUBLIC project_options retail_tick_clock)
endif()
target_sources(session_outer_capture PRIVATE
  "${PROJECT_SOURCE_DIR}/src/source_outer_scheduler.cpp"
  "${PROJECT_SOURCE_DIR}/src/source_outer_counter_win32.cpp")
target_link_libraries(session_outer_capture PUBLIC race_session_iteration source_input_session
  retail_scene_transition retail_frontend_clock session_effect_resources session_window_input kernel32)
# No race_session -> session_outer_capture edge: the new world state is a
# header-only owner. There is no static-target dependency cycle added here.
if(BUILD_TESTING)
  add_executable(source_outer_cadence_receiving "${PROJECT_SOURCE_DIR}/tests/source_outer_cadence_receiving.cpp")
  target_link_libraries(source_outer_cadence_receiving PRIVATE project_options retail_frontend_clock)
  add_test(NAME source_outer_cadence_receiving COMMAND source_outer_cadence_receiving)
  add_executable(source_outer_admission_receiving "${PROJECT_SOURCE_DIR}/tests/source_outer_admission_receiving.cpp")
  target_link_libraries(source_outer_admission_receiving PRIVATE session_outer_capture)
  add_test(NAME source_outer_admission_receiving COMMAND source_outer_admission_receiving)
endif()
