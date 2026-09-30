# Include from the NEW root AFTER Main applies Loader's window owner patch
# and includes source_input_cold_init.cmake and window_input_owner.cmake.
# No auto-patching, source copying or executable launch occurs at configure.
include_guard(GLOBAL)
if(NOT WIN32 OR NOT MSVC OR NOT BUILD_TESTING)
  message(FATAL_ERROR "Window Session receiving requires Windows/MSVC and BUILD_TESTING")
endif()
if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Window Session receiving is prepared for the Main x64 target")
endif()
foreach(required_target project_options session_window_input source_input_cold_init_session)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Window Session receiving requires ${required_target}")
  endif()
endforeach()
add_executable(window_input_session_receiving
  "${PROJECT_SOURCE_DIR}/tests/window_input_session/session_cases.cpp"
  "${PROJECT_SOURCE_DIR}/tests/window_input_session/native_boundary.cpp"
  "${PROJECT_SOURCE_DIR}/tests/window_input_session/allocation_probe.cpp")
target_link_libraries(window_input_session_receiving PRIVATE
  session_window_input source_input_cold_init_session user32 kernel32)
target_compile_definitions(window_input_session_receiving PRIVATE
  DIRECTINPUT_VERSION=0x0800
  SGN_WINDOW_SESSION_STARTUP_SOURCE="${PROJECT_SOURCE_DIR}/src/menu_directinput_startup_win32.cpp"
  SGN_WINDOW_SESSION_KEYBOARD_SOURCE="${PROJECT_SOURCE_DIR}/src/menu_keyboard_win32.cpp")
# These two actual implementation TUs are included by native_boundary.cpp.
# Their archive objects must remain unselected: no /WHOLEARCHIVE or duplicate
# direct source inclusion. Other keyboard/acquisition objects link normally.
option(SGN_RUN_WINDOW_INPUT_SESSION_RECEIVING "Register eight real-window Session cases" OFF)
if(SGN_RUN_WINDOW_INPUT_SESSION_RECEIVING)
  foreach(case RANGE 1 8)
    add_test(NAME window_input_session_B${case} COMMAND window_input_session_receiving
      "${PROJECT_SOURCE_DIR}/data/retail" "B${case}")
    set_tests_properties(window_input_session_B${case} PROPERTIES
      TIMEOUT 240 RUN_SERIAL TRUE LABELS "window;session;scripted-COM;input")
  endforeach()
endif()
