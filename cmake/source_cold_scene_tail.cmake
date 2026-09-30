include_guard(GLOBAL)
# Include after cold-scene v1, main_menu_runtime_children and Camera fullentry.
# Models source_cold_camera_initialization.cpp is ALREADY in race_session via
# main_menu_runtime_children. Do not add that TU to another target.
foreach(required IN ITEMS source_cold_scene_resources race_session polygon_device_context
    source_world_track_view source_menu_reconstruction source_model_resource_preparation source_track_part_initialization retail_scene_transition retail_viewport)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "source cold tail requires existing ${required}")
  endif()
endforeach()
target_sources(source_cold_scene_resources PRIVATE
    "${PROJECT_SOURCE_DIR}/src/source_cold_scene_tail.cpp"
    "${PROJECT_SOURCE_DIR}/src/source_cold_scene_lights.cpp"
    "${PROJECT_SOURCE_DIR}/src/source_track_contact_pointers.cpp"
    "${PROJECT_SOURCE_DIR}/src/source_scene_lights_x64.asm"
    "${PROJECT_SOURCE_DIR}/src/source_cold_menu_scene_services.cpp")
target_link_libraries(source_cold_scene_resources PUBLIC polygon_device_context
    source_world_track_view source_menu_reconstruction source_model_resource_preparation source_track_part_initialization retail_scene_transition retail_viewport)
# Existing lighting/device/submission/export/reset CPPs keep their own targets.
# Main explicitly links this target into its application and invokes its adapter.
