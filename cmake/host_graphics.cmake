include_guard(GLOBAL)
# NEW HOST graphics settings (R22). Not recovered behaviour: presentation
# options only (filtering, anti-aliasing, interpolation, aspect).
add_library(host_graphics STATIC src/host_graphics_settings.cpp)
target_link_libraries(host_graphics PUBLIC project_options)
foreach(consumer platform_d3d12 polygon_d3d12 source_menu_reconstruction source_application_runtime StuntGP_NEW)
  if(TARGET ${consumer})
    target_link_libraries(${consumer} PUBLIC host_graphics)
  endif()
endforeach()
