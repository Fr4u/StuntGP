cmake_minimum_required(VERSION 3.25)
project(StuntGPWorldPolygonPipeline LANGUAGES CXX)
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(world_polygon_pipeline
 "${SGN_ROOT}/tests/world_polygon_pipeline.cpp"
 "${SGN_ROOT}/src/retail_world_polygons.cpp"
 "${SGN_ROOT}/src/retail_world_polygon_producer.cpp"
 "${SGN_ROOT}/src/model_resources.cpp" "${SGN_ROOT}/src/source_pmd_model.cpp" "${SGN_ROOT}/src/source_allocation_bank.cpp"
 "${SGN_ROOT}/src/assets.cpp")
target_include_directories(world_polygon_pipeline PRIVATE "${SGN_ROOT}/include")
if(MSVC)
 target_compile_options(world_polygon_pipeline PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
else()
 target_compile_options(world_polygon_pipeline PRIVATE -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
endif()
