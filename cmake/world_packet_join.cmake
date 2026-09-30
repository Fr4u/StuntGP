# NEW optional CPU diagnostics. No activation, device creation or automatic run.
if(NOT TARGET session_world_geometry OR NOT TARGET polygon_device_context)
  return()
endif()
include_guard(GLOBAL)
add_library(world_packet_join STATIC "${PROJECT_SOURCE_DIR}/src/world_packet_join.cpp")
target_link_libraries(world_packet_join PUBLIC polygon_device_context)
add_library(session_world_join STATIC "${PROJECT_SOURCE_DIR}/src/session_world_join.cpp")
target_link_libraries(session_world_join PUBLIC world_packet_join race_session)
target_link_libraries(session_world_geometry PUBLIC session_world_join)
target_link_libraries(session_world_submission PUBLIC session_world_join)
if(BUILD_TESTING)
  add_executable(world_packet_join_cpu_tests "${PROJECT_SOURCE_DIR}/tests/world_packet_join_cpu.cpp")
  target_link_libraries(world_packet_join_cpu_tests PRIVATE world_packet_join)
  add_test(NAME world_packet_join_cpu COMMAND world_packet_join_cpu_tests)
endif()

# Application-layer diagnostics; the existing writer stays in race_application.
add_library(world_join_application STATIC
  "${PROJECT_SOURCE_DIR}/src/world_join_options.cpp"
  "${PROJECT_SOURCE_DIR}/src/world_join_sha256.cpp"
  "${PROJECT_SOURCE_DIR}/src/world_join_sidecar.cpp")
target_link_libraries(world_join_application PUBLIC world_packet_join)
if(TARGET race_application)
  target_link_libraries(race_application PRIVATE world_join_application)
endif()
if(BUILD_TESTING)
  add_executable(world_join_application_cpu_tests
    "${PROJECT_SOURCE_DIR}/tests/world_join_application_cpu.cpp"
    "${PROJECT_SOURCE_DIR}/src/displayed_recipe_export.cpp")
  target_link_libraries(world_join_application_cpu_tests PRIVATE world_join_application)
  add_test(NAME world_join_application_cpu COMMAND world_join_application_cpu_tests)
endif()
