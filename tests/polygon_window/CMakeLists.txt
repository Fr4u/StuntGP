cmake_minimum_required(VERSION 3.25)
project(PolygonWindow LANGUAGES CXX)
enable_testing()
add_executable(polygon_window_tests
  ../polygon_window.cpp ../../src/renderer.cpp ../../src/polygon_d3d12.cpp
  ../../src/assets.cpp ../../src/model_resources.cpp ../../src/source_pmd_model.cpp ../../src/source_allocation_bank.cpp ../../src/live_scene_geometry.cpp
  ../../src/retail_camera_transform.cpp ../../src/retail_camera.cpp
  ../../src/retail_model_matrices.cpp ../../src/retail_world_view_prefix.cpp)
target_include_directories(polygon_window_tests PRIVATE ../../include)
target_compile_features(polygon_window_tests PRIVATE cxx_std_23)
target_compile_definitions(polygon_window_tests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(polygon_window_tests PRIVATE /W4 /WX /fp:strict /permissive- /utf-8 /EHsc)
target_link_libraries(polygon_window_tests PRIVATE d3d12 dxgi d3dcompiler dxguid gdi32)
if(NOT DEFINED PROBE_OUTPUT)
  message(FATAL_ERROR "PROBE_OUTPUT must name a fresh evidence directory")
endif()
add_test(NAME polygon_window_warp COMMAND polygon_window_tests --warp "${PROBE_OUTPUT}/warp")
add_test(NAME polygon_window_hardware COMMAND polygon_window_tests --hardware "${PROBE_OUTPUT}/hardware")
set_tests_properties(polygon_window_warp polygon_window_hardware PROPERTIES TIMEOUT 240 RUN_SERIAL TRUE)
