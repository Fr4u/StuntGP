include_guard(GLOBAL)
# Registration only. Core/Composition must supply the actual required source
# children and admission; this fragment does not enable a race route.
foreach(required_target source_application_runtime race_session
    session_outer_frame_services source_world_frame_consumers source_world_lighting
    retail_race_status_text retail_score_popup retail_frame_border
    retail_screen_polygon_submission retail_pause_age_fade retail_loading_frame)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Source race draw requires actual target ${required_target}")
  endif()
endforeach()
target_sources(source_application_runtime PRIVATE
    src/source_core_race_draw.cpp
    src/source_texture_motion.cpp
    src/source_texture_motion_x64.asm)
target_link_libraries(source_application_runtime PUBLIC
    session_outer_frame_services source_world_frame_consumers source_world_lighting
    retail_race_status_text retail_score_popup retail_frame_border
    retail_screen_polygon_submission retail_pause_age_fade retail_loading_frame)
