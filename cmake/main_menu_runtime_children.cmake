include_guard(GLOBAL)
# Integrated source children and explicit Windows host transports. This file
# registers code only; the application still needs its retained composition.
target_sources(race_session PRIVATE
  src/source_sprite_scene.cpp src/source_sprite_allocations.cpp
  src/source_menu_camera_geometry.cpp src/source_cold_camera_initialization.cpp)
target_link_libraries(race_session PUBLIC retail_camera_vehicle source_frontend_packed_word)
target_link_libraries(source_music_runtime PUBLIC source_boot_config_native)
target_link_libraries(retail_camera_initialization PUBLIC retail_camera_vehicle)
target_sources(source_application_literals PRIVATE src/source_transient_material_names.cpp)
add_library(retail_frontend_language_selection STATIC src/retail_frontend_language_selection.cpp)
target_link_libraries(retail_frontend_language_selection PUBLIC
  project_options source_frontend_catalog retail_frontend_font_metrics)
target_sources(source_menu_reconstruction PRIVATE src/source_selection_data.cpp
  src/source_audio_options_page.cpp src/source_selection_page_draw.cpp
  src/source_selection_draw_x64.asm src/source_menu_language_binding.cpp
  src/source_save_page_draw.cpp src/source_demo_race.cpp src/source_setup_page.cpp
  src/source_language_page.cpp src/source_language_draw_x64.asm src/source_multi_page.cpp
  src/source_catalog_page.cpp src/source_catalog_draw_x64.asm
  src/source_records_pages.cpp src/source_records_draw_x64.asm
  src/source_unlock_page.cpp src/source_unlock_draw_x64.asm)
target_link_libraries(source_menu_reconstruction PUBLIC retail_frontend_language_selection)
# Concrete menu exit uses Native CRT in this target; keep the dependency
# runtime -> source_menu_reconstruction, with no reverse link or duplicate TU.
target_sources(source_application_runtime PRIVATE
  src/source_main_menu_children.cpp
  src/source_application_core.cpp src/source_application_menu_composition.cpp
  src/source_score_configuration.cpp src/source_configuration_file.cpp
  src/source_gameplay_configuration.cpp src/source_gameplay_scale_x64.asm
  src/source_menu_crt_exit.cpp src/source_crt_file_flush.cpp src/source_crt_close.cpp
  src/source_crt_descriptor_io.cpp src/source_crt_read.cpp src/source_crt_buffer.cpp src/source_crt_startup_io.cpp
  src/source_crt_stream_acquire.cpp
  src/source_crt_binary_open.cpp
  src/source_native_crt.cpp src/source_native_crt_fpu_x64.asm)

# Concrete retained menu/startup composition; source children keep their existing targets.
target_link_libraries(source_application_runtime PUBLIC source_scene_reset_composition source_script_number)

# Source fread and overlap-copy use the retained CRT buffer provider.
target_sources(source_application_runtime PRIVATE src/source_crt_fread.cpp)

# Source fwrite and single-byte output share the existing CRT owners.
target_sources(source_application_runtime PRIVATE src/source_crt_fwrite.cpp)

# Source replay compressor only; no recording/game-loop activation.
# Reuses this target's existing43D9B0 leaf and retained Frontend storage.
target_sources(retail_frontend_initialization PRIVATE src/source_replay_compress.cpp)

#44DC5B invocation adapter shares439E90's native identity representation.
#No loop or phase activation; codec stays in its existing target.
target_sources(source_menu_reconstruction PRIVATE src/source_replay_selected.cpp)

#43A910 uses existing menu/race/configuration owners; no44D740 loop activation.
target_sources(source_menu_reconstruction PRIVATE src/source_time_trial_launch.cpp)

#Full44D740 phase11; dispatcher activation still requires its remaining phases.
target_sources(source_menu_reconstruction PRIVATE src/source_time_trial_save.cpp)

# Actual TimeTrial phases4/5 and saved replay decoder. Other phases remain required.
target_sources(source_menu_reconstruction PRIVATE src/source_time_trial_phases.cpp
  src/source_replay_decompress.cpp src/source_replay_normalize_x64.asm)

#Full TimeTrial44D740 body; routed through the actual Core frontend services.
target_sources(source_menu_reconstruction PRIVATE src/source_time_trial_mode.cpp)

#Full PageEnterName9 and its source keyboard/edit/draw children.
target_sources(source_menu_reconstruction PRIVATE src/source_enter_name_page.cpp src/source_enter_name_x64.asm)

#Source UV traversal and finite-coordinate x87 correction; not warm-activated.
target_sources(source_menu_reconstruction PRIVATE src/source_scene_uv.cpp src/source_scene_uv_x64.asm)

#Full source mode1 and its actual43A2A0 roster producer; Start25 keeps scene work.
target_sources(source_menu_reconstruction PRIVATE src/source_championship_mode.cpp)
target_link_libraries(source_menu_reconstruction PUBLIC retail_championship)

#Full PageCredits30 with the original loaded language and text command queue.
target_sources(source_menu_reconstruction PRIVATE src/source_credits_page.cpp)

# Full PageOptionsCFG16 and PageOptionsCTL19, using existing source owners.
target_sources(source_menu_reconstruction PRIVATE src/source_control_configuration_pages.cpp)

#Full mode5 campaign with its actual source result owners.
target_sources(source_menu_reconstruction PRIVATE src/source_campaign_mode.cpp)
target_link_libraries(source_menu_reconstruction PUBLIC retail_campaign_launch)

#Source StuntChallenge44D4C0, real43AE00 launch, PageTextSplash38 and shared45BE20.
target_sources(source_menu_reconstruction PRIVATE src/source_stunt_challenge_mode.cpp src/source_text_splash_page.cpp)

#Complete PageBmpSplash37, including original x87 fraction and sprite animation.
target_sources(source_menu_reconstruction PRIVATE src/source_bmp_splash_page.cpp src/source_bmp_splash_x64.asm)

#One43AE00 producer for the campaign and standalone stunt source callers.
target_sources(source_menu_reconstruction PRIVATE src/source_challenge_launch.cpp)

#Full PageSlot28, source saved campaigns and existing MenuText/font/draw owners.
target_sources(source_menu_reconstruction PRIVATE src/source_campaign_slot_page.cpp)

#Full selector31 PageArcadeScore and438740 alpha child on the same renderer.
target_sources(source_menu_reconstruction PRIVATE src/source_arcade_score_page.cpp src/source_arcade_score_x64.asm)

#Full PageSelectTree12: championship branches, original tree draw and MenuText.
target_sources(source_menu_reconstruction PRIVATE src/source_select_tree_page.cpp)

#Actual402420 startup. Existing cold scene2 entry and shared controller tail stay in race_session.
target_sources(race_session PRIVATE src/source_camera_spline.cpp)
target_sources(source_application_literals PRIVATE src/source_camera_literals.cpp)
target_sources(source_application_runtime PRIVATE src/source_camera_startup.cpp src/source_camera_store_x64.asm)

#PageCampaignMenu14: source presentation, car exchange, save and exit actions.
#Selector7 is an independent required page, never a dummy-success callback.
target_sources(source_menu_reconstruction PRIVATE src/source_campaign_menu_page.cpp)

#Full selector13 PageSelectTeam, sharing the live campaign/frontend owners.
target_sources(source_menu_reconstruction PRIVATE src/source_select_team_page.cpp src/source_select_team_x64.asm)
target_link_libraries(source_menu_reconstruction PUBLIC retail_campaign_presentation)

#Full selector32 PageCampaignScore on actual result/standings/text owners.
target_sources(source_menu_reconstruction PRIVATE src/source_campaign_score_page.cpp src/source_campaign_score_x64.asm)

#Full PageModify7, live campaign upgrades and existing frontend draw owners.
target_sources(source_menu_reconstruction PRIVATE src/source_modify_page.cpp src/source_modify_x64.asm)
target_link_libraries(source_menu_reconstruction PUBLIC retail_campaign_presentation)
