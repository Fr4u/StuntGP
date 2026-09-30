include_guard(GLOBAL)
# NEW HOST presentation (wide_hud R20): 2D overlay edge anchoring on the
# widened canvas (shared model A, sgn/host_wide_layout.hpp). Not recovered
# behaviour. retail_polygon_submission is the lowest 2D target linked by every
# chokepoint (432E00 quads, 4291F0 text, 447840 lines, 4428A0/445800 loading
# quads, 431830 border). Include after CMakeLists.txt defines
# retail_polygon_submission (:182) and after wide_3d's cmake (host_wide_layout).
if(NOT TARGET retail_polygon_submission)
  message(FATAL_ERROR "wide_hud requires the existing retail_polygon_submission target")
endif()
target_sources(retail_polygon_submission PRIVATE src/host_wide_overlay.cpp)
# host_wide_overlay.cpp calls host_wide::widened_view (wide_layout).
if(TARGET host_wide_layout)
  target_link_libraries(retail_polygon_submission PUBLIC host_wide_layout)
endif()
