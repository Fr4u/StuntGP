include_guard(GLOBAL)
# R17 remaining race draw children (4473F0/449FB0/40E030) and update demo 40DE30.
# Registration only; Composition constructs SourceCoreRaceDrawRemaining.
foreach(required_target source_application_runtime race_session retail_pause_menu
    retail_pause_lifecycle retail_results_submission retail_race_hud_submission
    retail_race_hud retail_scene_transition retail_demo_tail
    source_frontend_material_binding session_outer_frame_services)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Source remaining race draw requires ${required_target}")
  endif()
endforeach()
target_sources(source_application_runtime PRIVATE
    src/source_race_pause_results_draw.cpp
    src/source_core_race_draw_remaining.cpp
    src/source_core_race_demo_x64.asm)
target_link_libraries(source_application_runtime PUBLIC retail_pause_menu
    retail_pause_lifecycle retail_results_submission retail_race_hud_submission
    retail_race_hud retail_scene_transition retail_demo_tail
    source_frontend_material_binding session_outer_frame_services)
