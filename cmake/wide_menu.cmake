include_guard(GLOBAL)
# NEW HOST widescreen frontend presentation (wide_menu R20). Not recovered behaviour.
# Include after cmake/host_presentation.cmake and after the wide_layout (wide_3d)
# cmake that builds src/host_wide_layout.cpp (sgn/host_wide_layout.hpp).
foreach(required_target project_options polygon_submission host_profiler platform_d3d12
    renderer_loading_surfaces source_menu_reconstruction source_menu_presentation_backend)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "wide_menu requires existing target ${required_target}")
  endif()
endforeach()
# Host tags of the tiled background / full-screen frame / 4428A0 overlay, the
# recipe association and the presentation copy (sgn/host_wide_menu.hpp).
add_library(host_wide_menu STATIC src/host_wide_menu.cpp)
target_link_libraries(host_wide_menu PUBLIC project_options polygon_submission host_profiler)
# host_wide::current() (wide_layout). Use its own target when it has one,
# otherwise the renderer target that compiles host_wide_layout.cpp.
if(TARGET host_wide_layout)
  target_link_libraries(host_wide_menu PUBLIC host_wide_layout)
else()
  target_link_libraries(host_wide_menu PUBLIC platform_d3d12)
endif()
# Hooks: 438500/437F40/437EC0 (command queue/consumption), 43F6B0/43FF90/441D70 (scopes).
target_link_libraries(source_menu_reconstruction PUBLIC host_wide_menu)
# Hooks: BeginScene/draw/EndScene of the presentation backend (tags + remember).
target_link_libraries(source_menu_presentation_backend PUBLIC host_wide_menu)
# Hooks: stage_back / resize restore rasterize the widened presentation copy.
target_link_libraries(renderer_loading_surfaces PUBLIC host_wide_menu)
