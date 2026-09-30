include_guard(GLOBAL)
# One existing material/font owner, source scene2 loading and D3D12 upload.
target_sources(session_world_materials PRIVATE
  src/source_frontend_material_entry.cpp src/source_menu_material_context.cpp
  src/source_next_scene_adapters.cpp)
add_library(source_frontend_material_binding STATIC src/source_frontend_material_binding.cpp)
target_link_libraries(source_frontend_material_binding PUBLIC
  project_options session_world_materials material_texture_replacement source_menu_reconstruction)
# Compile the bridge as part of the game; actual menu composition must retain it.
target_link_libraries(source_application_runtime PUBLIC source_frontend_material_binding)
# Actual source442810 update composition; missing source-world children remain
# explicit providers. Does not construct a preview Session or activate drawing.
target_sources(source_application_runtime PRIVATE src/source_core_race_update.cpp)
target_link_libraries(source_application_runtime PUBLIC source_input_session
  retail_frontend_clock race_session_iteration session_outer_capture)
