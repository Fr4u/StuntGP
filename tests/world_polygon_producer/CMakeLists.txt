cmake_minimum_required(VERSION 3.24)
project(WorldPolygonProducer LANGUAGES CXX)
add_executable(world_polygon_producer ../world_polygon_producer.cpp ../../src/retail_world_polygon_producer.cpp
    ../../src/model_resources.cpp ../../src/source_pmd_model.cpp ../../src/source_allocation_bank.cpp ../../src/assets.cpp)
target_include_directories(world_polygon_producer PRIVATE ../../include)
target_compile_features(world_polygon_producer PRIVATE cxx_std_23)
target_compile_options(world_polygon_producer PRIVATE /W4 /WX /fp:strict /permissive- /utf-8)
