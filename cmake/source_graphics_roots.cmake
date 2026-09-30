include_guard(GLOBAL)
# Include once AFTER Models menu-material-context v1 and main_loader_integration.
# Models owns source_menu_material_context.cpp; do not add that source twice.
foreach(required IN ITEMS source_application_literals session_world_materials retail_material_reload)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "source_graphics_roots requires ${required}")
  endif()
endforeach()
target_sources(source_application_literals PRIVATE src/source_graphics_root_literals.cpp)
target_sources(session_world_materials PRIVATE src/source_menu_material_roots.cpp)
target_link_libraries(session_world_materials PUBLIC source_application_literals)
