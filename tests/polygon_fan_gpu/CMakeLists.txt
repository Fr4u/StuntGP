cmake_minimum_required(VERSION 3.25)
project(PolygonFanGpu LANGUAGES CXX)
add_library(fan_gpu STATIC
  ../../src/renderer.cpp ../../src/polygon_d3d12.cpp ../../src/assets.cpp
  ../../src/model_resources.cpp ../../src/source_pmd_model.cpp ../../src/source_allocation_bank.cpp ../../src/live_scene_geometry.cpp
  ../../src/retail_camera_transform.cpp ../../src/retail_camera.cpp
  ../../src/retail_model_matrices.cpp ../../src/retail_world_view_prefix.cpp
  ../../src/polygon_device_context.cpp ../../src/polygon_command_capture.cpp
  ../../src/retail_screen_polygon_submission.cpp ../../src/retail_screen_polygons.cpp
  ../../src/retail_viewport.cpp)
target_include_directories(fan_gpu PUBLIC ../../include)
target_compile_features(fan_gpu PUBLIC cxx_std_23)
target_compile_definitions(fan_gpu PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(fan_gpu PUBLIC /W4 /WX /fp:strict /permissive- /utf-8 /EHsc)
target_link_libraries(fan_gpu PUBLIC d3d12 dxgi d3dcompiler dxguid gdi32)
add_executable(polygon_fan_gpu_tests ../polygon_fan_gpu.cpp)
target_link_libraries(polygon_fan_gpu_tests PRIVATE fan_gpu)
add_executable(polygon_window_tests ../polygon_window.cpp)
target_link_libraries(polygon_window_tests PRIVATE fan_gpu)
