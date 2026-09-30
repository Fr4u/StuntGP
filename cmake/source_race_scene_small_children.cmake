include_guard(GLOBAL)
if(NOT TARGET race_session)
  message(FATAL_ERROR "Source scene children require the existing platform target")
endif()
target_sources(race_session PRIVATE src/source_race_scene_small_children.cpp src/source_race_scene_small_children_x64.asm)

# Actual bounded CRT POW; same platform/CRT, no scene activation.
target_sources(race_session PRIVATE src/source_scene_crt_power.cpp src/source_scene_crt_power_x64.asm)
