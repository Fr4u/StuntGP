cmake_minimum_required(VERSION 3.25)
project(SceneAudioPauseReceiving LANGUAGES CXX)
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(SGN_BUILD_RUNTIME OFF CACHE BOOL "" FORCE)
add_subdirectory("${SGN_ROOT}" runtime EXCLUDE_FROM_ALL)
add_executable(scene_audio_pause_tests "${SGN_ROOT}/tests/scene_audio_pause_tests.cpp")
# Runtime is OFF: source_boot_config is not declared by the root in this mode.
# Keep the actual world constructor/deleter and their source-bank dependencies.
target_sources(scene_audio_pause_tests PRIVATE
  "${SGN_ROOT}/src/source_boot_config.cpp"
  "${SGN_ROOT}/src/source_boot_config_files.cpp")
target_link_libraries(scene_audio_pause_tests PRIVATE source_allocation_bank)
add_executable(scene_transition_oracle "${SGN_ROOT}/tests/scene_transition_oracle.cpp")
add_executable(frontend_state_oracle "${SGN_ROOT}/tests/frontend_state_oracle.cpp")
target_link_libraries(scene_audio_pause_tests PRIVATE retail_scene_transition retail_vehicle_storage
  retail_audio retail_gameplay_configuration retail_game_text retail_vehicle_strip
  retail_world_polygon_frame world_model_draw)
target_link_libraries(scene_transition_oracle PRIVATE retail_scene_transition)
target_link_libraries(frontend_state_oracle PRIVATE retail_frontend_state)
add_custom_target(receiving ALL DEPENDS scene_audio_pause_tests scene_transition_oracle frontend_state_oracle)
enable_testing()
if(NOT DEFINED RECEIVING_ARCHIVE)
  message(FATAL_ERROR "RECEIVING_ARCHIVE required: execute archived binaries")
endif()
add_test(NAME scene_audio_pause COMMAND "${RECEIVING_ARCHIVE}/scene_audio_pause_tests.exe")
add_test(NAME scene_result_regression COMMAND "${RECEIVING_ARCHIVE}/scene_transition_oracle.exe" "${RECEIVING_ARCHIVE}/scene-current.dat")
add_test(NAME frontend_state_regression COMMAND "${RECEIVING_ARCHIVE}/frontend_state_oracle.exe" "${RECEIVING_ARCHIVE}/frontend-current.dat")
