# Private candidate. Main integrates after Root's source_allocation_bank and race_session.
include_guard(GLOBAL)
if(NOT TARGET source_allocation_bank OR NOT TARGET race_session)
  message(FATAL_ERROR "Integrate shared source_allocation_bank and race_session before boot config")
endif()
add_library(source_boot_config STATIC
  "${PROJECT_SOURCE_DIR}/src/source_boot_config.cpp"
  "${PROJECT_SOURCE_DIR}/src/source_boot_config_files.cpp")
target_include_directories(source_boot_config PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(source_boot_config PUBLIC project_options source_allocation_bank)
#RaceSessionState owns boot_config; its constructor and raw-cache deleter are
#defined in this core library. Every race_session consumer needs this edge.
target_link_libraries(race_session PUBLIC source_boot_config)
add_library(source_boot_config_session STATIC "${PROJECT_SOURCE_DIR}/src/source_boot_config_session.cpp")
target_link_libraries(source_boot_config_session PUBLIC source_boot_config race_session)
if(WIN32)
  add_library(source_boot_config_native STATIC "${PROJECT_SOURCE_DIR}/src/source_boot_config_native.cpp")
  target_link_libraries(source_boot_config_native PUBLIC source_boot_config)
endif()
if(BUILD_TESTING)
  add_executable(source_boot_config_tests "${PROJECT_SOURCE_DIR}/tests/source_boot_config_tests.cpp")
  target_link_libraries(source_boot_config_tests PRIVATE source_boot_config)
  add_test(NAME source_boot_config_receiving COMMAND source_boot_config_tests)
endif()
