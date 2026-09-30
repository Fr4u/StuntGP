# WP-B (native-tick-collision-r18), NEW host code: live-allocation track collision
# producer (decode_track_collision(const SourcePmdTrackView&), in the existing
# track_contact.cpp postimage) and the per-level owner SourceTrackCollision.
# Include from the root CMakeLists.txt after the `if(WIN32 AND SGN_BUILD_RUNTIME)`
# block that defines race_session (i.e. after its endif()).
include_guard(GLOBAL)
# track_contact.cpp now also reads the checked SourcePmdTrackView projections.
# No cycle: source_pmd_model links only source_allocation_bank and retail_assets.
target_link_libraries(track_contact PUBLIC source_pmd_model)
if(TARGET race_session)
  # Same target as race_session_platform.cpp; borrows RaceSessionPlatform,
  # SceneResources and ModelResources (scene_resources is already PUBLIC there).
  target_sources(race_session PRIVATE src/source_track_collision.cpp)
endif()
