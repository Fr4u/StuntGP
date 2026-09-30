cmake_minimum_required(VERSION 3.24)
project(WorldTrackParts LANGUAGES CXX)
add_executable(world_track_parts ../world_track_parts.cpp ../../src/assets.cpp ../../src/model_resources.cpp ../../src/source_pmd_model.cpp ../../src/source_allocation_bank.cpp
    ../../src/retail_world_polygon_producer.cpp ../../src/retail_world_local_camera.cpp ../../src/retail_world_track_parts.cpp
    ../../src/retail_world_model_lighting.cpp ../../src/retail_world_model_visibility.cpp ../../src/retail_model_matrices.cpp
    ../../src/retail_camera_transform.cpp ../../src/retail_camera.cpp ../../src/retail_track_part_initialization.cpp
    ../../src/retail_track_part_preparation.cpp)
target_include_directories(world_track_parts PRIVATE ../../include)
target_compile_features(world_track_parts PRIVATE cxx_std_23)
target_compile_options(world_track_parts PRIVATE /W4 /WX /fp:strict /permissive- /utf-8)
