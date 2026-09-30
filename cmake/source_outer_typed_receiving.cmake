# Private receiving only. Main is the sole compiler/native/GPU executor.
include_guard(GLOBAL)
foreach(required_target race_application_polygon_backend renderer_loading_surfaces session_outer_frame_services source_input_cold_init_session session_window_input race_session_pause_restart)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "typed receiving requires actual ${required_target}")
  endif()
endforeach()
set(SGN_TYPED_HEADER "${PROJECT_SOURCE_DIR}/include/sgn/race_application_polygon_backend.hpp")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SGN_TYPED_HEADER}")
file(SHA256 "${SGN_TYPED_HEADER}" SGN_TYPED_INPUT_SHA)
if(NOT SGN_TYPED_INPUT_SHA STREQUAL "fe08df5beb7736d27b10869b5c025943d85b5c412698650be0cd8aa24ddaf557")
  message(FATAL_ERROR "typed receiving backend header differs from approved preimage")
endif()
file(READ "${SGN_TYPED_HEADER}" SGN_TYPED_TEXT)
string(REPLACE "\r\n" "\n" SGN_TYPED_TEXT "${SGN_TYPED_TEXT}")
string(REGEX MATCHALL "class RaceApplicationPolygonBackend final [{]" SGN_TYPED_HITS "${SGN_TYPED_TEXT}")
list(LENGTH SGN_TYPED_HITS SGN_TYPED_COUNT)
if(NOT SGN_TYPED_COUNT EQUAL 1)
  message(FATAL_ERROR "typed receiving friend anchor is not unique")
endif()
string(REPLACE "class RaceApplicationPolygonBackend final {\n"
  "class RaceApplicationPolygonBackend final {\n    friend struct TypedOuterReceivingAccess;\n"
  SGN_TYPED_TEXT "${SGN_TYPED_TEXT}")
set(SGN_TYPED_OBSERVED "${CMAKE_CURRENT_BINARY_DIR}/typed-outer-observed")
file(MAKE_DIRECTORY "${SGN_TYPED_OBSERVED}/sgn")
file(CONFIGURE OUTPUT "${SGN_TYPED_OBSERVED}/sgn/race_application_polygon_backend.hpp"
  CONTENT "${SGN_TYPED_TEXT}" @ONLY NEWLINE_STYLE LF)
file(SHA256 "${SGN_TYPED_OBSERVED}/sgn/race_application_polygon_backend.hpp" SGN_TYPED_OUTPUT_SHA)
if(NOT SGN_TYPED_OUTPUT_SHA STREQUAL "21acaf23a4187b5e7a40837333415894e5712476d3cb46a71415b9183b3c5bab")
  message(FATAL_ERROR "typed receiving generated header differs from one-friend review")
endif()
add_executable(source_outer_typed_receiving
  "${PROJECT_SOURCE_DIR}/tests/source_outer_typed/typed_receiving.cpp"
  "${PROJECT_SOURCE_DIR}/src/race_application_polygon_backend.cpp"
  "${PROJECT_SOURCE_DIR}/tests/source_outer_session/native_boundary.cpp")
target_include_directories(source_outer_typed_receiving BEFORE PRIVATE "${SGN_TYPED_OBSERVED}"
  "${PROJECT_SOURCE_DIR}/tests/source_outer_typed" "${PROJECT_SOURCE_DIR}/tests/window_input_session" "${PROJECT_SOURCE_DIR}/tests/source_outer_session")
# Every TU gets the SAME class definition, including actual backend.cpp. No
# private/public macro, layout change, duplicate object, or game body override.
target_compile_options(source_outer_typed_receiving PRIVATE "/FI${SGN_TYPED_OBSERVED}/sgn/race_application_polygon_backend.hpp")
target_compile_definitions(source_outer_typed_receiving PRIVATE DIRECTINPUT_VERSION=0x0800
  SGN_WINDOW_SESSION_STARTUP_SOURCE="${PROJECT_SOURCE_DIR}/src/menu_directinput_startup_win32.cpp"
  SGN_WINDOW_SESSION_KEYBOARD_SOURCE="${PROJECT_SOURCE_DIR}/src/menu_keyboard_win32.cpp")
target_link_libraries(source_outer_typed_receiving PRIVATE race_application_polygon_backend renderer_loading_surfaces
  source_input_cold_init_session session_window_input race_session_pause_restart user32 kernel32)
# No /WHOLEARCHIVE. The explicit actual backend/native objects satisfy their
# symbols; duplicate original archive members must remain unselected.
option(SGN_RUN_SOURCE_OUTER_TYPED_RECEIVING "Register actual typed Session and WARP receiving" OFF)
if(SGN_RUN_SOURCE_OUTER_TYPED_RECEIVING)
foreach(group services_identity font_guard typed_null typed_stop typed_exception)
  add_test(NAME source_outer_typed_${group} COMMAND source_outer_typed_receiving "${PROJECT_SOURCE_DIR}/data/retail" "${group}")
  set_tests_properties(source_outer_typed_${group} PROPERTIES RUN_SERIAL TRUE TIMEOUT 600)
endforeach()
endif()
