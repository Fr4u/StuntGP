# Retail input ports with explicit Session invocation. Window startup and outer
# loop cadence are supplied by the application, never by a constructor or draw.
include_guard(GLOBAL)

add_library(menu_input_producer STATIC "${PROJECT_SOURCE_DIR}/src/menu_input_producer.cpp")
target_link_libraries(menu_input_producer PUBLIC project_options)
add_library(menu_keyboard_acquisition STATIC
  "${PROJECT_SOURCE_DIR}/src/menu_keyboard_acquisition.cpp"
  "${PROJECT_SOURCE_DIR}/src/menu_keyboard_win32.cpp")
target_link_libraries(menu_keyboard_acquisition PUBLIC menu_input_producer user32 kernel32)
add_library(menu_directinput_acquisition STATIC
  "${PROJECT_SOURCE_DIR}/src/menu_directinput_acquisition.cpp"
  "${PROJECT_SOURCE_DIR}/src/menu_directinput_win32.cpp")
target_link_libraries(menu_directinput_acquisition PUBLIC menu_keyboard_acquisition)
target_compile_definitions(menu_directinput_acquisition PRIVATE DIRECTINPUT_VERSION=0x0800)
add_library(menu_directinput_startup STATIC "${PROJECT_SOURCE_DIR}/src/menu_directinput_startup_win32.cpp")
target_link_libraries(menu_directinput_startup PUBLIC menu_directinput_acquisition user32 kernel32)
target_compile_definitions(menu_directinput_startup PRIVATE DIRECTINPUT_VERSION=0x0800)

# Storage must not depend on the Session dispatcher: race_session owns it.
add_library(source_input_storage STATIC "${PROJECT_SOURCE_DIR}/src/source_input_storage.cpp")
target_link_libraries(source_input_storage PUBLIC project_options)
target_link_libraries(race_session PUBLIC source_input_storage)
add_library(source_input_dispatch STATIC "${PROJECT_SOURCE_DIR}/src/source_input_dispatch.cpp")
target_link_libraries(source_input_dispatch PUBLIC source_input_storage menu_input_producer retail_pause_toggle)
add_library(source_input_session STATIC "${PROJECT_SOURCE_DIR}/src/source_input_session.cpp")
target_link_libraries(source_input_session PUBLIC race_session race_session_pause
  source_input_dispatch menu_directinput_acquisition retail_pause_input)

if(BUILD_TESTING)
  add_executable(source_input_dispatch_tests "${PROJECT_SOURCE_DIR}/tests/source_input_dispatch_tests.cpp")
  target_link_libraries(source_input_dispatch_tests PRIVATE source_input_session)
  add_executable(source_input_ownership_tests "${PROJECT_SOURCE_DIR}/tests/source_input_ownership_tests.cpp")
  target_link_libraries(source_input_ownership_tests PRIVATE source_input_session)
  add_test(NAME source_input_dispatch_session COMMAND source_input_dispatch_tests "${PROJECT_SOURCE_DIR}/data/retail")
  add_test(NAME source_input_session_ownership COMMAND source_input_ownership_tests "${PROJECT_SOURCE_DIR}/data/retail")
  set_tests_properties(source_input_dispatch_session source_input_session_ownership
    PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE)
endif()
