include_guard(GLOBAL)
if(NOT TARGET race_session)
 message(FATAL_ERROR "device bootstrap requires race_session and menu presentation backend")
endif()
target_sources(race_session PRIVATE
 "${PROJECT_SOURCE_DIR}/src/source_renderer_reset.cpp"
 "${PROJECT_SOURCE_DIR}/src/source_renderer_bootstrap.cpp")
