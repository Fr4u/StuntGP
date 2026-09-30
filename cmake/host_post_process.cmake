include_guard(GLOBAL)
# NEW HOST (R28) HUD split and opt-in bloom: the 3D part of a frame gets FXAA and
# bloom, then the 2D HUD is drawn on top. Not recovered behaviour.
if(TARGET platform_d3d12)
  target_sources(platform_d3d12 PRIVATE src/host_post_process_d3d12.cpp)
endif()
if(TARGET polygon_d3d12)
  target_sources(polygon_d3d12 PRIVATE src/host_frame_phase.cpp)
endif()
