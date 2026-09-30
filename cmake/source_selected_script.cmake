# Include after source_application_literals, source_cold_language and race_session.
include_guard(GLOBAL)
foreach(required IN ITEMS source_application_literals source_cold_language race_session)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "source_selected_script requires existing ${required}")
  endif()
endforeach()
add_library(source_selected_script STATIC "${PROJECT_SOURCE_DIR}/src/source_selected_script.cpp")
target_link_libraries(source_selected_script PUBLIC source_application_literals source_cold_language race_session)
