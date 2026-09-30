# NEW App lifetime owner for the exact bounded writable PE literal region.
# Source-only proposal: compilation/receiving remains a separate Main decision.
include_guard(GLOBAL)
add_library(source_application_literals STATIC src/source_application_literals.cpp)
target_link_libraries(source_application_literals PUBLIC project_options)
if(TARGET race_session)
  target_link_libraries(race_session PUBLIC source_application_literals)
endif()
