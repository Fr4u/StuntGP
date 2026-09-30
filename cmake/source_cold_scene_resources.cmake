# Include after main_loader_integration and the existing reset-prefix targets.
# This fragment adds no startup call, test, host backend or second source bank.
include_guard(GLOBAL)
foreach(required project_options race_session session_world_materials source_scene_resource_prefix
    source_scene_indexed_base source_scene_keyboard_prefix source_scene_packet_bases retail_world_view_prefix source_numeric_win64
    source_application_literals source_menu_presentation_backend session_outer_frame_services)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "source_cold_scene_resources requires existing ${required}")
  endif()
endforeach()
add_library(source_cold_scene_resources STATIC
    "${PROJECT_SOURCE_DIR}/src/source_scene_resource_reset.cpp"
    "${PROJECT_SOURCE_DIR}/src/source_cold_scene_resources.cpp"
    "${PROJECT_SOURCE_DIR}/src/source_cold_frontend_assets.cpp"
    "${PROJECT_SOURCE_DIR}/src/source_cold_scene_startup.cpp")
target_include_directories(source_cold_scene_resources PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(source_cold_scene_resources PUBLIC project_options race_session
    session_world_materials source_scene_resource_prefix source_scene_indexed_base
    source_scene_keyboard_prefix source_scene_packet_bases retail_world_view_prefix source_numeric_win64
    source_application_literals source_menu_presentation_backend session_outer_frame_services)
