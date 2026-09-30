include_guard(GLOBAL)
# NEW HOST pacing (R18 P2) and rebased source counter (PORT-FIX proposal P1).
# Include after cmake/new_host_d3d12_application_surfaces.cmake.
foreach(required_target source_application_runtime platform_d3d12)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Host pacing requires ${required_target}")
  endif()
endforeach()
target_sources(source_application_runtime PRIVATE
  src/source_host_counter_win32.cpp
  src/source_host_pacing_win32.cpp)
target_link_libraries(source_application_runtime PUBLIC platform_d3d12 PRIVATE user32 kernel32)

target_link_libraries(source_application_runtime PUBLIC host_profiler)
