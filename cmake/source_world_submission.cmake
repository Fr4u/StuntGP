include_guard(GLOBAL)
# Register the actual source packet consumer. This does not activate Core draw.
foreach(required_target source_application_runtime race_session session_outer_frame_services)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Source world submission requires actual target ${required_target}")
  endif()
endforeach()
target_sources(source_application_runtime PRIVATE
  src/source_world_submission.cpp src/source_world_submission_x64.asm)
target_link_libraries(source_application_runtime PUBLIC race_session session_outer_frame_services)
