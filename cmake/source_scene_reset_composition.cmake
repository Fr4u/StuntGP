if(NOT TARGET session_outer_capture)
  return()
endif()
include_guard(GLOBAL)
add_library(source_scene_reset_composition STATIC
  "${PROJECT_SOURCE_DIR}/src/source_scene_reset_composition.cpp")
target_link_libraries(source_scene_reset_composition PUBLIC project_options race_session retail_frontend_clock retail_scene_transition source_cold_language_input)
target_link_libraries(session_outer_capture PUBLIC source_scene_reset_composition)
include("${CMAKE_CURRENT_LIST_DIR}/source_scene_reset_composition_receiving.cmake")
