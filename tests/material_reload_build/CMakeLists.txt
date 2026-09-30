cmake_minimum_required(VERSION 3.25)
project(StuntGPMaterialReloadQualification LANGUAGES CXX)
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(material_reload_tests ../material_reload_tests.cpp
 ../../src/retail_material_reload.cpp ../../src/retail_font_resources.cpp ../../src/source_allocation_bank.cpp)
target_include_directories(material_reload_tests PRIVATE "${SGN_ROOT}/include")
target_compile_options(material_reload_tests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
enable_testing()
add_test(NAME material_reload_source COMMAND material_reload_tests
 "${SGN_ROOT}/reports/level-work/series6/material-reload-source-audit-v1/reload.dat")
