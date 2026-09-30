# Source pause state transitions. CPU component qualification and current-root
# receiving are separate from attachment to the persistent Session world.
include_guard(GLOBAL)

add_library(retail_pause_music STATIC "${PROJECT_SOURCE_DIR}/src/retail_pause_music.cpp")
target_link_libraries(retail_pause_music PUBLIC project_options)

add_library(retail_pause_hud STATIC "${PROJECT_SOURCE_DIR}/src/retail_pause_hud.cpp")
target_link_libraries(retail_pause_hud PUBLIC project_options)

add_library(retail_pause_lifecycle STATIC "${PROJECT_SOURCE_DIR}/src/retail_pause_lifecycle.cpp")
target_link_libraries(retail_pause_lifecycle PUBLIC
  retail_pause_hud retail_pause_music retail_race_hud retail_audio)
