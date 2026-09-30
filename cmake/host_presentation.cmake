include_guard(GLOBAL)
# NEW HOST high-refresh presentation (interp R19). Not recovered behaviour.
# Include after cmake/source_world_submission.cmake (every target below exists).
foreach(required_target race_session source_application_runtime source_menu_presentation_backend
    source_world_frame_consumers)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Host presentation requires ${required_target}")
  endif()
endforeach()
# Read-only tag/snapshot bookkeeping used by the backend, the core race draw and
# the world producers/submission hooks.
add_library(host_presentation STATIC src/host_presentation_tags.cpp)
target_link_libraries(host_presentation PUBLIC project_options race_session)
target_link_libraries(source_menu_presentation_backend PUBLIC host_presentation)
target_link_libraries(source_world_frame_consumers PUBLIC host_presentation)
target_link_libraries(source_application_runtime PUBLIC host_presentation)
# The presenter builds interpolated copies of the canonical capture and presents
# them through Renderer::present_polygon_submission_transient (perf_gpu R19).
target_sources(source_application_runtime PRIVATE src/host_presentation_presenter.cpp)
