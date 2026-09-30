include_guard(GLOBAL)
add_library(playable_menu_input STATIC
  "${PROJECT_SOURCE_DIR}/src/playable_menu_input.cpp"
  "${PROJECT_SOURCE_DIR}/src/playable_menu_input_session.cpp")
target_link_libraries(playable_menu_input PUBLIC race_session_pause retail_pause_toggle retail_frontend_launch
  menu_keyboard_acquisition source_input_storage)
if(BUILD_TESTING)
  add_executable(playable_menu_input_tests "${PROJECT_SOURCE_DIR}/tests/playable_menu_input_tests.cpp")
  target_link_libraries(playable_menu_input_tests PRIVATE playable_menu_input)
  add_test(NAME playable_menu_host_input COMMAND playable_menu_input_tests)
endif()
