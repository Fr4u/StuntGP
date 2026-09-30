include_guard(GLOBAL)
# R18 WP-A/WP-C (native-tick-splines-r18, DESIGN A1/A7): spline resolution and
# the track spline owners for the native 418A90 modules.
#
# Resolving a live source spline binding needs the actual platform
# (RaceSessionPlatform, race_session). The native modules that read spline
# bindings (contact_rail, retail_ai_automatic, contact_ai_stunts via it,
# retail_ai_rival, retail_vehicle_tick) therefore link track_spline_resolution.
# race_session -> retail_vehicle_tick_composition -> contact_rail ->
# track_spline_resolution -> race_session is an intended STATIC-library cycle
# (CMake repeats cyclic static libraries; the MSVC linker resolves it).
# Include this file after race_session, race_session_world_tick and
# cmake/source_rail_reset_leaf_receiving.cmake have been defined.
if(NOT TARGET race_session)
  message(FATAL_ERROR "native_tick_splines.cmake requires the race_session target")
endif()

add_library(track_spline_resolution STATIC src/spline_resolution.cpp)
target_link_libraries(track_spline_resolution PUBLIC race_session source_pmd_spline)

# SourceTrackSplines: 428800/4192F0/418B10/405260 on the platform's single world.
target_sources(race_session PRIVATE src/source_track_splines.cpp)
target_link_libraries(race_session PUBLIC track_spline_resolution)

# DecodedTrackSplines: decoded file form (tests, tools, preview START until WP-F).
add_library(decoded_track_splines STATIC src/decoded_track_splines.cpp)
target_link_libraries(decoded_track_splines PUBLIC contact_rail retail_ai_automatic retail_spline)

target_link_libraries(contact_rail PUBLIC track_spline_resolution retail_spline)
target_link_libraries(retail_ai_automatic PUBLIC track_spline_resolution)
target_link_libraries(retail_vehicle_tick PUBLIC track_spline_resolution retail_ai_automatic)
target_link_libraries(race_session_world_tick PUBLIC decoded_track_splines)

# Test users of DecodedTrackSplines.
target_link_libraries(contact_rail_oracle PRIVATE decoded_track_splines)
target_link_libraries(vehicle_ai_automatic_oracle PRIVATE decoded_track_splines)
target_link_libraries(vehicle_ai_automatic_tests PRIVATE decoded_track_splines)
foreach(sgn_spline_test source_rail_reset_leaf_receiving source_rail_reset_rail_regression source_rail_reset_full_tick_regression)
  if(TARGET ${sgn_spline_test})
    target_link_libraries(${sgn_spline_test} PRIVATE decoded_track_splines)
  endif()
endforeach()
