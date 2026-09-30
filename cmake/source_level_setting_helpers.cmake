include_guard(GLOBAL)
foreach(required IN ITEMS source_application_literals race_session)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "source_level_setting_helpers requires existing ${required}")
  endif()
endforeach()
add_library(source_level_setting_helpers STATIC
  "${PROJECT_SOURCE_DIR}/src/source_level_kind_compare.cpp"
  "${PROJECT_SOURCE_DIR}/src/source_level_setting_helpers_session.cpp")
target_include_directories(source_level_setting_helpers PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(source_level_setting_helpers PUBLIC source_application_literals race_session)
