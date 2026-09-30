cmake_minimum_required(VERSION 3.25)
project(pause_meter_native LANGUAGES CXX)
if(NOT MSVC OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
 message(FATAL_ERROR "Meter native executor requires MSVC x64")
endif()
add_executable(pause_meter_native ../native.cpp ../../../src/retail_pause_meter.cpp
 ../../../src/retail_text_submission.cpp ../../../src/retail_font_resources.cpp ../../../src/source_allocation_bank.cpp
 ../../../src/retail_quad_submission.cpp ../../../src/retail_ui_line.cpp
 ../../../src/retail_screen_polygon_submission.cpp ../../../src/assets.cpp)
target_include_directories(pause_meter_native PRIVATE ../../../include)
target_compile_features(pause_meter_native PRIVATE cxx_std_23)
target_compile_options(pause_meter_native PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
enable_testing()
add_test(NAME pause_meter_contracts COMMAND pause_meter_native --contracts)
if(PAUSE_METER_CORPUS)
 add_test(NAME pause_meter_source COMMAND pause_meter_native "${PAUSE_METER_CORPUS}")
endif()
