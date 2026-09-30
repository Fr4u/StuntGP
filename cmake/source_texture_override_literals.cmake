#Append this source to the SAME App target; no second literal owner/library.
include_guard(GLOBAL)
if(NOT TARGET source_application_literals)
  message(FATAL_ERROR "Integrate source_application_literals before its texture override reads")
endif()
target_sources(source_application_literals PRIVATE
  "${PROJECT_SOURCE_DIR}/src/source_texture_override_literals.cpp")
