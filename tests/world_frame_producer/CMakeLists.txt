cmake_minimum_required(VERSION 3.24)
project(WorldFrameProducer LANGUAGES CXX)
add_executable(world_frame_producer ../world_frame_producer.cpp
    ../../src/assets.cpp ../../src/model_resources.cpp ../../src/source_pmd_model.cpp ../../src/source_allocation_bank.cpp
    ../../src/retail_world_frame_producer.cpp ../../src/retail_world_track_base.cpp
    ../../src/retail_world_track_parts.cpp ../../src/retail_world_model_lists.cpp
    ../../src/retail_world_background_models.cpp ../../src/retail_world_strip_producer.cpp
    ../../src/retail_world_sprite_producer.cpp ../../src/retail_world_polygon_frame.cpp
    ../../src/retail_world_polygons.cpp ../../src/retail_world_polygon_producer.cpp
    ../../src/retail_world_spatial_producer.cpp ../../src/retail_world_local_camera.cpp
    ../../src/retail_world_model_producer.cpp ../../src/retail_world_lit_model_producer.cpp
    ../../src/retail_world_model_lighting.cpp ../../src/retail_world_model_visibility.cpp
    ../../src/retail_world_model_detail.cpp ../../src/retail_model_matrices.cpp
    ../../src/retail_camera_transform.cpp ../../src/retail_camera.cpp
    ../../src/retail_track_part_initialization.cpp ../../src/retail_track_part_preparation.cpp
    ../../src/retail_track_geometry_animation.cpp ../../src/retail_track_normal_allocation.cpp
    ../../src/track_part_animation.cpp ../../src/contact_particle_allocator.cpp
    ../../src/world_model_draw.cpp ../../src/retail_model_draw_queue.cpp
    ../../src/retail_vehicle_strip.cpp ../../src/retail_vehicle_strip_draw.cpp
    ../../src/retail_light_effects.cpp ../../src/retail_lighting.cpp)
target_include_directories(world_frame_producer PRIVATE ../../include)
target_compile_features(world_frame_producer PRIVATE cxx_std_23)
target_compile_options(world_frame_producer PRIVATE /W4 /WX /fp:strict /permissive- /utf-8)
enable_testing()
if(DEFINED FRAME_CORPUS)
    add_test(NAME world_frame_full_source_parity COMMAND world_frame_producer "${CMAKE_CURRENT_SOURCE_DIR}/../../data/retail" "${FRAME_CORPUS}")
endif()
