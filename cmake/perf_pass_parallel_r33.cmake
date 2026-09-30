include_guard(GLOBAL)
# perf-pass-parallel R33 (NEW host performance). Include after cmake/host_texture_upscale.cmake (which
# defines host_texture_upscale and adds src/host_parallel.cpp to it). The race-pass vertex kernels in
# source_world_track_vertices.cpp (target source_world_track_view) and source_world_frame_consumers.cpp
# use sgn::host_parallel (detail/pass_parallel.hpp); declare that dependency for the test executables that
# link these targets without platform_d3d12. The new yield points use only the inline
# host_presentation::pass_yield_point() (no link dependency). New header: include/sgn/detail/pass_parallel.hpp.
foreach(target_name IN ITEMS source_world_track_view source_world_frame_consumers)
  if(TARGET host_texture_upscale AND TARGET ${target_name})
    target_link_libraries(${target_name} PUBLIC host_texture_upscale)
  endif()
endforeach()
