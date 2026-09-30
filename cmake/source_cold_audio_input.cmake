include_guard(GLOBAL)
if(NOT WIN32 OR NOT SGN_BUILD_RUNTIME)
  return()
endif()
set(SGN_COLD_AUDIO_INPUT_DEPS source_audio_initialization_tail
    source_audio_enumeration_session source_audio_default_device_session
    source_audio_cooperative_platform source_audio_primary_platform
    source_audio_primary_format_platform source_audio_listener_acquisition_platform
    source_audio_listener_native_configuration_platform source_input_cold_init_session)
foreach(required IN LISTS SGN_COLD_AUDIO_INPUT_DEPS)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "cold audio/input composition requires integrated ${required}")
  endif()
endforeach()
add_library(source_cold_audio_input STATIC
    "${PROJECT_SOURCE_DIR}/src/source_cold_audio.cpp"
    "${PROJECT_SOURCE_DIR}/src/source_cold_audio_input.cpp")
target_link_libraries(source_cold_audio_input PUBLIC ${SGN_COLD_AUDIO_INPUT_DEPS})
if(BUILD_TESTING)
  add_executable(source_cold_audio_input_tests "${PROJECT_SOURCE_DIR}/tests/source_cold_audio_input_tests.cpp")
  target_link_libraries(source_cold_audio_input_tests PRIVATE source_cold_audio_input)
  set(SGN_COLD_AUDIO_INPUT_ASSETS "${PROJECT_SOURCE_DIR}/data/retail" CACHE PATH "Retail assets for CPU audio/input composition")
  add_test(NAME source_cold_audio_input_cpu COMMAND source_cold_audio_input_tests "${SGN_COLD_AUDIO_INPUT_ASSETS}")
  set_tests_properties(source_cold_audio_input_cpu PROPERTIES RUN_SERIAL TRUE TIMEOUT 30)
endif()
