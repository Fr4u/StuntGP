include_guard(GLOBAL)
# perf-tick R30 (NEW host performance). Include after the host_profiler target is defined, e.g. right
# after include(cmake/perf_world_r19.cmake). The new SGN_PROFILE_SCOPE markers in these targets use the
# NEW host profiler; declare that dependency where the target does not already reach it through
# platform_d3d12 / source_application_runtime (test executables link these libraries without them).
# polygon_command_capture gained no profiler use; its new <mutex> needs nothing extra.
foreach(target_name IN ITEMS source_allocation_bank retail_world_material_loading source_world_spatial_emission
                             source_menu_reconstruction)
  if(TARGET host_profiler AND TARGET ${target_name})
    target_link_libraries(${target_name} PUBLIC host_profiler)
  endif()
endforeach()
