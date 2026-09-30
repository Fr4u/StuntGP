# Reached42C7CA call plus42C7CF sky writes, stopping before42C809->42C580.
include_guard(GLOBAL)
if(TARGET source_sky_request_prefix)
  add_library(source_sky_motion_prefix STATIC src/source_sky_motion_prefix.cpp)
  target_link_libraries(source_sky_motion_prefix PUBLIC source_sky_request_prefix source_request_search)
endif()
