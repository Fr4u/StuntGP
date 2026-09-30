# Include in Main's approved receiving build after add_subdirectory(NEW).
include_guard(GLOBAL)
foreach(required_target session_outer_capture session_outer_source_bindings source_input_cold_init_session session_window_input race_session_pause_restart)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "source outer receiving requires actual ${required_target}")
  endif()
endforeach()
# Generate the observation TU from the actual production source. Refuse a
# changed preimage until its single observation point is reviewed again.
set(SGN_OUTER_DEVICE_SOURCE "${PROJECT_SOURCE_DIR}/src/polygon_device_context.cpp")
file(SHA256 "${SGN_OUTER_DEVICE_SOURCE}" SGN_OUTER_DEVICE_HASH)
if(NOT SGN_OUTER_DEVICE_HASH STREQUAL "13ca32a9b9873ea69953283bd6d1010e1a6944a19dc5283982d869164f753e39")
  message(FATAL_ERROR "Review the source outer receiving observation against the changed device source")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${SGN_OUTER_DEVICE_SOURCE}")
file(READ "${SGN_OUTER_DEVICE_SOURCE}" SGN_OUTER_DEVICE_TEXT)
string(REPLACE "\r\n" "\n" SGN_OUTER_DEVICE_TEXT "${SGN_OUTER_DEVICE_TEXT}")
set(SGN_OUTER_OBSERVE_ANCHOR "    auto next=std::make_unique<Impl::Capture>(impl_->state,tick,epoch,limits);impl_->capture=std::move(next);")
string(FIND "${SGN_OUTER_DEVICE_TEXT}" "${SGN_OUTER_OBSERVE_ANCHOR}" SGN_OUTER_OBSERVE_POSITION)
if(SGN_OUTER_OBSERVE_POSITION LESS 0)
  message(FATAL_ERROR "Missing source outer capture observation anchor")
endif()
string(REPLACE "${SGN_OUTER_OBSERVE_ANCHOR}" "${SGN_OUTER_OBSERVE_ANCHOR}\n    source_outer_receiving::observe_capture_begin(tick,epoch);" SGN_OUTER_DEVICE_TEXT "${SGN_OUTER_DEVICE_TEXT}")
set(SGN_OUTER_OBSERVED "${CMAKE_CURRENT_BINARY_DIR}/source-outer-session-observed/polygon_device_context.cpp")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/source-outer-session-observed")
file(WRITE "${SGN_OUTER_OBSERVED}" "${SGN_OUTER_DEVICE_TEXT}")
add_executable(source_outer_session_receiving
  "${PROJECT_SOURCE_DIR}/tests/source_outer_session/session_receiving.cpp"
  "${PROJECT_SOURCE_DIR}/tests/source_outer_session/native_boundary.cpp"
  "${SGN_OUTER_OBSERVED}")
target_link_libraries(source_outer_session_receiving PRIVATE session_outer_capture session_outer_source_bindings
  source_input_cold_init_session session_window_input race_session_pause_restart user32 kernel32)
target_include_directories(source_outer_session_receiving PRIVATE "${PROJECT_SOURCE_DIR}/tests/source_outer_session"
  "${PROJECT_SOURCE_DIR}/tests/window_input_session")
target_compile_definitions(source_outer_session_receiving PRIVATE DIRECTINPUT_VERSION=0x0800
  SGN_WINDOW_SESSION_STARTUP_SOURCE="${PROJECT_SOURCE_DIR}/src/menu_directinput_startup_win32.cpp"
  SGN_WINDOW_SESSION_KEYBOARD_SOURCE="${PROJECT_SOURCE_DIR}/src/menu_keyboard_win32.cpp")
# Forced declaration keeps the observed TU diff to ONE observer statement.
target_compile_options(source_outer_session_receiving PRIVATE "/FI${PROJECT_SOURCE_DIR}/tests/source_outer_session/capture_probe.hpp")
# No /WHOLEARCHIVE: these three actual TU objects satisfy the symbols before
# unmodified libraries; their duplicate archive members must remain unselected.
option(SGN_RUN_SOURCE_OUTER_SESSION_RECEIVING "Register actual Session and WARP receiving" OFF)
if(SGN_RUN_SOURCE_OUTER_SESSION_RECEIVING)
foreach(group no_catchup paused_iteration warm_stop duplicate foreign_session uninitialized_input foreign_input foreign_device_capture
  warm_active_overlay release_idle_shared release_idle_separate release_foreign_backend release_capture_guards release_pending release_generation release_stale_world release_suspended)
  add_test(NAME source_outer_session_${group} COMMAND source_outer_session_receiving
    "${PROJECT_SOURCE_DIR}/data/retail" "${group}")
  set_tests_properties(source_outer_session_${group} PROPERTIES TIMEOUT 600 RUN_SERIAL TRUE)
endforeach()
endif()
