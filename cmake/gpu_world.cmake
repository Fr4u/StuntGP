include_guard(GLOBAL)
# NEW host rendering (gpu_world R19): hybrid GPU world-geometry path.
# Include AFTER cmake/source_reconstruction_prefix.cmake (source_world_track_view)
# and cmake/source_world_submission.cmake (source_application_runtime).
foreach(required_target source_world_track_view source_application_runtime)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "gpu_world requires target ${required_target}")
  endif()
endforeach()
# Runtime switch (SGN_GPU_WORLD) and backend availability flag.
add_library(gpu_world_mode STATIC src/gpu_world_mode.cpp)
target_link_libraries(gpu_world_mode PUBLIC project_options)
# 42ACF2 side: per-level typed PMD parse, per-view ticket, reference_project.
target_sources(source_world_track_view PRIVATE src/gpu_world_track.cpp)
target_link_libraries(source_world_track_view PUBLIC gpu_world_mode)
# 433F40 side (TrackRun) and renderer side (perf_gpu R19 insertion API:
# Renderer::create_static_buffer, PolygonDeviceContext::insert_gpu_command).
# source_application_runtime already links platform_d3d12 and the presentation backend.
target_sources(source_application_runtime PRIVATE src/gpu_world_submission.cpp src/gpu_world_d3d12.cpp)
target_link_libraries(source_application_runtime PUBLIC gpu_world_mode source_world_track_view d3dcompiler)
