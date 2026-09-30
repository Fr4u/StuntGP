include_guard(GLOBAL)
foreach(required IN ITEMS race_session source_application_literals source_time_trial_selection source_car_selection source_level_overrides)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "source_track_replay requires existing ${required}")
  endif()
endforeach()
add_library(source_track_replay STATIC "${PROJECT_SOURCE_DIR}/src/source_track_replay.cpp")
target_include_directories(source_track_replay PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(source_track_replay PUBLIC race_session source_application_literals source_time_trial_selection source_car_selection source_level_overrides)

# Concrete 42D157 ->4453A0 adapter; reuses this target's complete children.
target_sources(source_track_replay PRIVATE
  "${PROJECT_SOURCE_DIR}/src/source_race_level_script.cpp")
