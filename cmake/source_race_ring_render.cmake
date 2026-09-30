include_guard(GLOBAL)
if(NOT TARGET race_session)
  message(FATAL_ERROR "Source ring render requires actual race_session target")
endif()
# source-race-view-stage-20260926 is a required reviewed prerequisite.
target_sources(race_session PRIVATE src/source_race_ring_render.cpp src/source_race_ring_render_x64.asm)

# NEW host transport has no RaceSessionPlatform methods or world copies.
# Consumers share one implementation without introducing a static link cycle.
add_library(source_ring_transport STATIC src/source_ring_x87_x64.asm src/source_screen_transport.cpp)
target_link_libraries(source_ring_transport PUBLIC project_options source_allocation_bank)
foreach(receiver race_session retail_screen_polygons retail_screen_polygon_submission retail_lighting retail_trail_packets)
  target_link_libraries(${receiver} PRIVATE source_ring_transport)
endforeach()
