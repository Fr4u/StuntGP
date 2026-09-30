include_guard(GLOBAL)
# Register only. Core/Composition activation remains with the caller.
foreach(required_target race_session source_application_runtime retail_trail_packets)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Source race view stage requires ${required_target}")
  endif()
endforeach()
target_sources(race_session PRIVATE
  src/source_race_view_stage.cpp src/source_race_view_stage_x64.asm)
target_sources(retail_trail_packets PRIVATE
  src/source_trail_packet_writer.cpp src/source_trail_packet_writer_x64.asm)
