# Source force feedback with the existing input and DirectInput lifetime.
# Include after cmake/source_input.cmake; this module performs no source startup.
include_guard(GLOBAL)
if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "Force-feedback receiving is qualified only for Windows/MSVC SDK interfaces")
endif()
if(NOT TARGET source_input_session OR NOT TARGET menu_directinput_startup)
  message(FATAL_ERROR "Integrate the actual Session input/startup targets before force feedback")
endif()
add_library(source_force_feedback STATIC "${PROJECT_SOURCE_DIR}/src/source_force_feedback.cpp")
target_link_libraries(source_force_feedback PUBLIC race_session menu_directinput_startup retail_vehicle)
target_compile_definitions(source_force_feedback PRIVATE DIRECTINPUT_VERSION=0x0800)
target_link_libraries(source_input_dispatch PUBLIC source_force_feedback)
target_link_libraries(source_input_session PUBLIC source_force_feedback)

# Main must rebuild the entire dependent graph: SourceInputStorage layout changes
# inside RaceSessionState. An old race_session library with a new header is invalid.
if(BUILD_TESTING)
  add_executable(source_force_feedback_receiving "${PROJECT_SOURCE_DIR}/tests/source_force_feedback/receiving.cpp")
  target_link_libraries(source_force_feedback_receiving PRIVATE source_input_session)
  target_compile_definitions(source_force_feedback_receiving PRIVATE
    DIRECTINPUT_VERSION=0x0800
    SGN_FORCE_FEEDBACK_STARTUP_SOURCE="${PROJECT_SOURCE_DIR}/src/menu_directinput_startup_win32.cpp")
  add_executable(source_force_feedback_startup_regression "${PROJECT_SOURCE_DIR}/tests/source_force_feedback/startup_regression.cpp")
  target_link_libraries(source_force_feedback_startup_regression PRIVATE source_input_storage menu_directinput_acquisition)
  target_compile_definitions(source_force_feedback_startup_regression PRIVATE
    DIRECTINPUT_VERSION=0x0800
    SGN_FORCE_FEEDBACK_STARTUP_SOURCE="${PROJECT_SOURCE_DIR}/src/menu_directinput_startup_win32.cpp")
  add_test(NAME source_force_feedback_scripted COMMAND source_force_feedback_receiving "${PROJECT_SOURCE_DIR}/data/retail")
  add_test(NAME source_force_feedback_startup_regression COMMAND source_force_feedback_startup_regression)
  set_tests_properties(source_force_feedback_scripted source_force_feedback_startup_regression
    PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE LABELS "scripted;input;force-feedback")
endif()
# Scripted executable includes the patched real startup TU after seven API-name
# seams. Do not compile that TU directly into the executable as well. In its
# dependency archive the startup object must remain unselected because its public
# symbols are supplied by the fixture TU; no /WHOLEARCHIVE for that library.
