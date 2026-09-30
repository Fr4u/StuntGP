# New host lifetime; include after cmake/source_input.cmake in the Windows
# runtime block. Core never links race_session or source_input_session.
include_guard(GLOBAL)
foreach(required_target project_options menu_directinput_startup race_session)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "window input owner requires target ${required_target}")
  endif()
endforeach()
add_library(window_input_owner STATIC "${PROJECT_SOURCE_DIR}/src/window_input_owner_win32.cpp")
target_link_libraries(window_input_owner PUBLIC project_options menu_directinput_startup user32 kernel32)
# Storage contains unique_ptr<WindowInputOwner>, whose destructor is out of line.
target_link_libraries(race_session PUBLIC window_input_owner)
add_library(session_window_input STATIC "${PROJECT_SOURCE_DIR}/src/session_window_input.cpp")
target_link_libraries(session_window_input PUBLIC race_session window_input_owner)

if(BUILD_TESTING)
  add_executable(window_input_owner_native_checks "${PROJECT_SOURCE_DIR}/tests/window_input_owner_native_checks.cpp")
  target_link_libraries(window_input_owner_native_checks PRIVATE window_input_owner)
  # A real hidden Win32 window and system dinput.dll are used. No source
  # startup/shutdown, enumeration, acquisition, keyboard polling or FF calls.
  option(SGN_RUN_WINDOW_INPUT_NATIVE_CHECKS "Register the real window ownership checks" OFF)
  if(SGN_RUN_WINDOW_INPUT_NATIVE_CHECKS)
    add_test(NAME window_input_owner_native_checks COMMAND window_input_owner_native_checks)
    set_tests_properties(window_input_owner_native_checks PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE)
  endif()
endif()
