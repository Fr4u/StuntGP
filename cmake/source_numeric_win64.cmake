# Include after the actual source_outer_scheduler integration. CPU-only
# production receiver; no Session ownership edge back into the numeric core.
include_guard(GLOBAL)
if(NOT WIN32 OR NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Source numeric receiver currently requires MSVC Windows AMD64")
endif()
if(NOT TARGET session_outer_capture)
  message(FATAL_ERROR "Integrate source outer scheduler before its numeric receiver")
endif()
enable_language(ASM_MASM)
add_library(source_numeric_control STATIC "${PROJECT_SOURCE_DIR}/src/source_numeric_control.cpp")
target_link_libraries(source_numeric_control PRIVATE project_options)
target_include_directories(source_numeric_control PUBLIC "${PROJECT_SOURCE_DIR}/include")
add_library(source_numeric_win64 STATIC
  "${PROJECT_SOURCE_DIR}/src/source_numeric_win64.cpp"
  "${PROJECT_SOURCE_DIR}/src/source_viewport_win64.cpp"
  "${PROJECT_SOURCE_DIR}/src/source_numeric_x64.asm")
target_include_directories(source_numeric_win64 PUBLIC "${PROJECT_SOURCE_DIR}/include")
# Avoid passing C++ /fp:/EHsc flags to ml64 through project_options.
target_link_libraries(source_numeric_win64 PRIVATE source_numeric_control)
target_compile_options(source_numeric_win64 PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/fp:strict;/utf-8>")
target_link_libraries(session_outer_capture PUBLIC source_numeric_win64)
if(BUILD_TESTING)
  add_executable(source_numeric_receiving "${PROJECT_SOURCE_DIR}/tests/source_numeric_receiving.cpp")
  target_link_libraries(source_numeric_receiving PRIVATE project_options source_numeric_control)
  add_test(NAME source_numeric_receiving COMMAND source_numeric_receiving "${PROJECT_SOURCE_DIR}/tests/data/source_numeric_control.bin")
  add_executable(source_numeric_native_receiving "${PROJECT_SOURCE_DIR}/tests/source_numeric_native_receiving.cpp")
  target_link_libraries(source_numeric_native_receiving PRIVATE project_options source_numeric_win64)
  option(SGN_RUN_SOURCE_NUMERIC_NATIVE "Register actual x87/MXCSR receiver checks" OFF)
  if(SGN_RUN_SOURCE_NUMERIC_NATIVE)
    add_test(NAME source_numeric_native_receiving COMMAND source_numeric_native_receiving)
  endif()
endif()

# Native424BD0 receiving is compile-only by default. Only Main supplies fresh
# output paths and the explicit --main-reviewed switch; no automatic CTest run.
if(BUILD_TESTING)
  add_executable(source_viewport_native_receiving
    "${PROJECT_SOURCE_DIR}/tests/source_viewport_native_receiving.cpp"
    "${PROJECT_SOURCE_DIR}/tests/source_viewport_test_seed.asm")
  target_link_libraries(source_viewport_native_receiving PRIVATE source_numeric_win64)
  target_compile_options(source_viewport_native_receiving PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/fp:strict;/utf-8>")
  foreach(point IN ITEMS 1 2)
    add_executable(source_viewport_unwind_receiving_${point}
      "${PROJECT_SOURCE_DIR}/tests/source_viewport_unwind_receiving.cpp"
      "${PROJECT_SOURCE_DIR}/tests/source_viewport_test_seed.asm"
      "${PROJECT_SOURCE_DIR}/src/source_viewport_win64.cpp"
      "${PROJECT_SOURCE_DIR}/src/source_numeric_x64.asm")
    target_include_directories(source_viewport_unwind_receiving_${point} PRIVATE "${PROJECT_SOURCE_DIR}/include")
    target_compile_definitions(source_viewport_unwind_receiving_${point} PRIVATE
      SGN_VIEWPORT_TEST_BREAK=${point} SGN_VIEWPORT_EXPECT_BREAK=${point})
    target_compile_options(source_viewport_unwind_receiving_${point} PRIVATE
      "$<$<COMPILE_LANGUAGE:CXX>:/W4;/WX;/permissive-;/EHsc;/fp:strict;/utf-8>")
  endforeach()
endif()
