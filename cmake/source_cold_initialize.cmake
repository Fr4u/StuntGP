#Include after all named existing cold child targets. No hidden source boot.
include_guard(GLOBAL)
set(SGN_COLD_INITIALIZE_CHILDREN race_session source_numeric_win64 source_cold_time_session
    session_outer_capture source_boot_config_session source_input_cold_init_session source_cold_language_input source_cold_audio_input)
foreach(required IN LISTS SGN_COLD_INITIALIZE_CHILDREN)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "source_cold_initialize requires existing ${required}")
  endif()
endforeach()
add_library(source_cold_initialize STATIC "${PROJECT_SOURCE_DIR}/src/source_cold_initialize.cpp")
target_include_directories(source_cold_initialize PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(source_cold_initialize PUBLIC ${SGN_COLD_INITIALIZE_CHILDREN})
