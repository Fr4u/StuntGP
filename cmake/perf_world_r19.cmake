include_guard(GLOBAL)
# perf_world R19 (NEW host performance). Include after cmake/source_reconstruction_prefix.cmake
# and after the host_profiler target is defined. The new SGN_PROFILE_SCOPE markers in
# src/source_world_track_view.cpp use the NEW host profiler; declare that dependency.
# (source_world_submission.cpp is in source_application_runtime, which already links it.)
# New headers sgn/source_bank_read_memo.hpp and sgn/detail/world_vertex_kernels.hpp are
# header-only. src/source_track_vertices_x64.asm and src/source_part_vertices_x64.asm stay
# registered: they are the SGN_WORLD_X87_REFERENCE diagnostic reference.
if(TARGET host_profiler AND TARGET source_world_track_view)
  target_link_libraries(source_world_track_view PUBLIC host_profiler)
endif()
