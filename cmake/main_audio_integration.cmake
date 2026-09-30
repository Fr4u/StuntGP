include_guard(GLOBAL)
enable_language(ASM_MASM)
add_library(source_music_runtime STATIC
  src/source_normal_race_initialize.cpp
  src/source_vehicle_bootstrap.cpp
  src/source_vehicle_bootstrap_x64.asm
  src/source_vehicle_clone.cpp
  src/source_race_config_receivers.cpp
  src/source_race_config_x64.asm
  src/source_initial_grid.cpp
  src/source_trial_start.cpp
  src/source_score_factory.cpp
  src/source_race_lifecycle.cpp
  src/source_race_lifecycle_x64.asm
  src/source_audio_shutdown.cpp
  src/source_music_buffer_directsound.cpp
  src/source_music_buffer_size.cpp
  src/source_music_buffer_size_x64.asm
  src/source_music_calls.cpp
  src/source_menu_music.cpp
  src/source_menu_sample_native.cpp
  src/source_menu_audio_maintenance.cpp
  src/source_music_disk_installed.cpp
  src/source_music_initialization.cpp
  src/source_music_levels.cpp
  src/source_music_open.cpp
  src/source_music_periodic.cpp
  src/source_music_request.cpp
  src/source_music_timer_caps_installed.cpp
  src/source_music_timer_dispatch.cpp
  src/source_music_timer_entry_prefix.cpp
  src/source_music_wave_open.cpp
  src/source_music_wave_winmm.cpp
  src/source_music_windows_runtime.cpp)
target_link_libraries(source_music_runtime PUBLIC
  project_options race_session source_application_literals source_boot_config
  source_cold_audio_input platform_audio winmm dsound user32)
# Existing consumers keep their original translation-unit owners. Their
# source branches now call this shared runtime, not a parallel preview registry.
target_link_libraries(race_session PUBLIC source_music_runtime)
target_link_libraries(retail_audio PUBLIC source_music_runtime)
# SourceMusicDiskDwordAccess lives in source_music_timer_entry_prefix.cpp.
# Do not add an older candidate TU or another45FDA0 initialization body.

# Source401770 and reached4011F0/401030 on the retained source world/sample owners.
target_sources(source_music_runtime PRIVATE src/source_score_popup_update.cpp src/source_score_popup_update_x64.asm)
