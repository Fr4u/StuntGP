cmake_minimum_required(VERSION 3.25)
project(StuntGPMaterialReloadReceiving LANGUAGES CXX)
# Configure only during the second lease, after explicit context/root migration.
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT EXISTS "${FONT_CORPUS}")
 message(FATAL_ERROR "Supply the unchanged qualified original font corpus through FONT_CORPUS")
endif()
add_executable(material_reload_receiving ../material_reload_receiving.cpp
 ../../src/retail_material_reload.cpp ../../src/retail_font_resources.cpp ../../src/source_allocation_bank.cpp
 ../../src/retail_world_material_loading.cpp ../../src/retail_world_materials.cpp
 ../../src/retail_material_filesystem.cpp ../../src/assets.cpp
 ../../src/session_effect_resources.cpp ../../src/retail_world_sprite_model.cpp)
target_include_directories(material_reload_receiving PRIVATE "${SGN_ROOT}/include")
target_compile_options(material_reload_receiving PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
enable_testing()
add_test(NAME material_reload_receiving_source COMMAND material_reload_receiving
 "${FONT_CORPUS}" "${SGN_ROOT}/data/retail")
