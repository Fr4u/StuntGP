include_guard(GLOBAL)
# NEW HOST (R34) autonomous displayed-FPS stutter detector and optional full trace.
if(TARGET host_profiler)
  target_sources(host_profiler PRIVATE src/host_frame_trace.cpp)
endif()
