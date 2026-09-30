cmake_minimum_required(VERSION 3.24)
project(WorldTrackBase LANGUAGES CXX)
add_executable(world_track_base ../world_track_base.cpp ../../src/assets.cpp ../../src/model_resources.cpp ../../src/source_pmd_model.cpp ../../src/source_allocation_bank.cpp
    ../../src/retail_world_track_base.cpp ../../src/retail_world_polygon_frame.cpp ../../src/retail_world_polygons.cpp
    ../../src/retail_world_polygon_producer.cpp ../../src/retail_world_spatial_producer.cpp ../../src/retail_world_local_camera.cpp
    ../../src/retail_model_matrices.cpp ../../src/retail_camera_transform.cpp ../../src/retail_camera.cpp
    ../../src/retail_track_part_initialization.cpp ../../src/retail_track_part_preparation.cpp
    ../../src/retail_track_geometry_animation.cpp ../../src/retail_track_normal_allocation.cpp
    ../../src/track_part_animation.cpp ../../src/contact_particle_allocator.cpp)
target_include_directories(world_track_base PRIVATE ../../include)
target_compile_features(world_track_base PRIVATE cxx_std_23)
target_compile_options(world_track_base PRIVATE /W4 /WX /fp:strict /permissive- /utf-8)
