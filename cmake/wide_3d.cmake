include_guard(GLOBAL)
# NEW HOST presentation (R20 widescreen, wide_layout/wide_3d). Not recovered
# behaviour. host_wide_layout owns the shared canvas model
# (include/sgn/host_wide_layout.hpp); the race Hor+ scope lives in race_session.
# Include after every target below exists (end of CMakeLists.txt).
add_library(host_wide_layout STATIC src/host_wide_layout.cpp)
target_link_libraries(host_wide_layout PUBLIC project_options)
if(TARGET retail_viewport)
  target_link_libraries(host_wide_layout PUBLIC retail_viewport)
endif()
foreach(consumer polygon_d3d12 platform_d3d12 race_session host_presentation source_application_runtime)
  if(TARGET ${consumer})
    target_link_libraries(${consumer} PUBLIC host_wide_layout)
  endif()
endforeach()
if(TARGET race_session)
  target_sources(race_session PRIVATE src/host_wide_race_view.cpp)
endif()
if(TARGET StuntGP_NEW)
  target_link_libraries(StuntGP_NEW PRIVATE host_wide_layout)
endif()
