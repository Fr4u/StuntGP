include_guard(GLOBAL)
if(NOT TARGET source_boot_config OR NOT TARGET race_session)
  message(FATAL_ERROR "Frontend raw memory requires the existing boot config and race session")
endif()
add_library(source_frontend_resource_memory STATIC
  "${PROJECT_SOURCE_DIR}/src/source_frontend_resource_memory.cpp")
target_link_libraries(source_frontend_resource_memory PUBLIC project_options source_boot_config)
target_link_libraries(race_session PUBLIC source_frontend_resource_memory)
if(BUILD_TESTING)
  add_executable(source_frontend_resource_memory_tests
    "${PROJECT_SOURCE_DIR}/tests/source_frontend_resource_memory_tests.cpp")
  target_link_libraries(source_frontend_resource_memory_tests PRIVATE source_frontend_resource_memory)
  add_test(NAME source_frontend_resource_memory_receiving COMMAND source_frontend_resource_memory_tests)
endif()
