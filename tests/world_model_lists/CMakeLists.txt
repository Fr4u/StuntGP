cmake_minimum_required(VERSION 3.24)
project(WorldModelLists LANGUAGES CXX)
add_executable(world_model_lists ../world_model_lists.cpp ../../src/assets.cpp ../../src/model_resources.cpp ../../src/source_pmd_model.cpp ../../src/source_allocation_bank.cpp
    ../../src/retail_world_model_lists.cpp ../../src/retail_world_polygon_frame.cpp ../../src/retail_world_polygons.cpp
    ../../src/retail_world_polygon_producer.cpp ../../src/retail_world_local_camera.cpp ../../src/retail_world_model_producer.cpp ../../src/retail_world_lit_model_producer.cpp
    ../../src/retail_world_model_lighting.cpp ../../src/retail_world_model_visibility.cpp ../../src/retail_world_model_detail.cpp
    ../../src/retail_model_matrices.cpp ../../src/retail_camera_transform.cpp ../../src/retail_camera.cpp)
target_include_directories(world_model_lists PRIVATE ../../include)
target_compile_features(world_model_lists PRIVATE cxx_std_23)
target_compile_options(world_model_lists PRIVATE /W4 /WX /fp:strict /permissive- /utf-8)
