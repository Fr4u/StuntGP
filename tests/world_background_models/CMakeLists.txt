cmake_minimum_required(VERSION 3.24)
project(WorldBackgroundModels LANGUAGES CXX)
add_library(background_core STATIC ../../src/assets.cpp ../../src/model_resources.cpp ../../src/source_pmd_model.cpp ../../src/source_allocation_bank.cpp
    ../../src/retail_world_background_models.cpp ../../src/retail_world_polygon_frame.cpp ../../src/retail_world_polygons.cpp
    ../../src/retail_world_polygon_producer.cpp ../../src/retail_world_local_camera.cpp ../../src/retail_world_model_producer.cpp
    ../../src/retail_world_model_visibility.cpp ../../src/retail_world_model_detail.cpp ../../src/retail_world_sprite_producer.cpp
    ../../src/retail_world_strip_producer.cpp ../../src/retail_vehicle_strip.cpp
    ../../src/world_model_draw.cpp ../../src/retail_model_draw_queue.cpp
    ../../src/retail_model_matrices.cpp ../../src/retail_camera_transform.cpp ../../src/retail_camera.cpp)
add_executable(world_background_models ../world_background_models.cpp)
add_executable(world_strip_producer ../world_strip_producer.cpp)
target_include_directories(background_core PUBLIC ../../include)
target_compile_features(background_core PUBLIC cxx_std_23)
target_compile_options(background_core PUBLIC /W4 /WX /fp:strict /permissive- /utf-8)
target_link_libraries(world_background_models PRIVATE background_core)
target_link_libraries(world_strip_producer PRIVATE background_core)
enable_testing()
add_test(NAME world_strip_binding_guards COMMAND world_strip_producer --contracts)
if(DEFINED BACKGROUND_CORPUS)
    add_test(NAME world_background_source_parity COMMAND world_background_models
        "${CMAKE_CURRENT_SOURCE_DIR}/../../data/retail" "${BACKGROUND_CORPUS}")
endif()
if(DEFINED STRIP_CORPUS)
    add_test(NAME world_strip_source_parity COMMAND world_strip_producer "${STRIP_CORPUS}")
endif()
