include_guard(GLOBAL)
foreach(required IN ITEMS race_session source_script_number source_level_setting_helpers)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "source_level_settings requires existing ${required}")
  endif()
endforeach()
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "source_level_settings x87 receiver requires native Windows AMD64/MSVC")
endif()
enable_language(ASM_MASM)
add_library(source_level_settings STATIC
  "${PROJECT_SOURCE_DIR}/src/source_level_settings.cpp"
  "${PROJECT_SOURCE_DIR}/src/source_level_settings_x64.asm")
target_include_directories(source_level_settings PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(source_level_settings PUBLIC
  race_session source_script_number source_level_setting_helpers)
