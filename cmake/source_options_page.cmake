include_guard(GLOBAL)
#Include after main_menu_integration, audio kind17 and source_start_mode2.
#43D500 belongs exclusively to Mechanic source_start_mode2.cpp.
#The existing audio CPP supplies route_frontend_audio_option_454e2a; do not
#duplicate its factory/descriptor/settings/apply owner in this target.
if(NOT TARGET source_menu_reconstruction)
  message(FATAL_ERROR "source_options_page requires source_menu_reconstruction")
endif()
target_sources(source_menu_reconstruction PRIVATE src/source_options_page.cpp)
#NEW HOST (R23) GRAPHICS options page (Options row + page); host_graphics.cmake
#links host_graphics into source_menu_reconstruction.
target_sources(source_menu_reconstruction PRIVATE src/host_graphics_page.cpp)

#NEW HOST (R24) shared GRAPHICS menu model (rows, values, strings, session
#save) used by the Options GRAPHICS page and the in-race pause GRAPHICS panel.
#NEW HOST (R29): procedural scroll arrows (vertex geometry through the polygon sink).
add_library(host_graphics_menu STATIC src/host_graphics_menu_model.cpp src/host_menu_arrow.cpp)
target_link_libraries(host_graphics_menu PUBLIC project_options host_graphics)
#retail_polygon_submission (:182) provides host_wide_overlay and, via wide_hud.cmake, host_wide_layout.
target_link_libraries(host_graphics_menu PUBLIC retail_polygon_submission)
target_link_libraries(source_menu_reconstruction PUBLIC host_graphics_menu)
if(TARGET retail_pause_menu)
  target_link_libraries(retail_pause_menu PUBLIC host_graphics_menu)
endif()
