cmake_minimum_required(VERSION 3.25)
project(FontMaterialD3D12Qualification LANGUAGES CXX)
enable_testing()
add_library(font_gpu_core STATIC
  ../../src/renderer.cpp ../../src/polygon_d3d12.cpp ../../src/material_d3d12.cpp
  ../../src/font_material_d3d12.cpp ../../src/retail_font_resources.cpp ../../src/source_allocation_bank.cpp ../../src/retail_frontend_font_metrics.cpp
  ../../src/material_texture_replacement.cpp ../../src/race_overlay_textures.cpp
  ../../src/assets.cpp ../../src/model_resources.cpp ../../src/source_pmd_model.cpp ../../src/live_scene_geometry.cpp
  ../../src/retail_camera_transform.cpp ../../src/retail_camera.cpp
  ../../src/retail_model_matrices.cpp ../../src/retail_world_view_prefix.cpp
  ../../src/retail_material_reload.cpp ../../src/retail_world_material_loading.cpp ../../src/retail_world_materials.cpp
  ../../src/retail_material_filesystem.cpp
  ../../src/session_effect_resources.cpp ../../src/retail_world_sprite_model.cpp
  ../../src/polygon_submission.cpp ../../src/sdr_gamma_d3d12.cpp)
target_include_directories(font_gpu_core PUBLIC ../../include)
target_compile_features(font_gpu_core PUBLIC cxx_std_23)
target_compile_definitions(font_gpu_core PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(font_gpu_core PUBLIC /W4 /WX /fp:strict /permissive- /utf-8 /EHsc)
target_link_libraries(font_gpu_core PUBLIC d3d12 dxgi d3dcompiler dxguid gdi32)
add_executable(font_material_d3d12_tests ../font_material_d3d12.cpp)
target_link_libraries(font_material_d3d12_tests PRIVATE font_gpu_core)
add_executable(material_texture_replacement_tests ../material_texture_replacement.cpp)
target_link_libraries(material_texture_replacement_tests PRIVATE font_gpu_core)
add_executable(material_d3d12_tests ../material_d3d12.cpp)
target_link_libraries(material_d3d12_tests PRIVATE font_gpu_core)
add_executable(polygon_window_tests ../polygon_window.cpp)
target_link_libraries(polygon_window_tests PRIVATE font_gpu_core)
if(NOT DEFINED PROBE_OUTPUT OR NOT DEFINED SOURCE_CORPUS OR NOT DEFINED REPLACEMENT_CORPUS)
  message(FATAL_ERROR "PROBE_OUTPUT, SOURCE_CORPUS and REPLACEMENT_CORPUS required")
endif()
get_filename_component(RETAIL_ROOT "../../data/retail" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
foreach(adapter warp hardware)
  add_test(NAME font_${adapter} COMMAND font_material_d3d12_tests --${adapter} "${RETAIL_ROOT}" "${SOURCE_CORPUS}" "${PROBE_OUTPUT}/font-${adapter}")
  add_test(NAME replacement_${adapter} COMMAND material_texture_replacement_tests --${adapter} "${RETAIL_ROOT}" "${REPLACEMENT_CORPUS}" "${PROBE_OUTPUT}/replacement-${adapter}")
  add_test(NAME material_${adapter} COMMAND material_d3d12_tests --${adapter} "${RETAIL_ROOT}" "${PROBE_OUTPUT}/material-${adapter}")
  add_test(NAME window_${adapter} COMMAND polygon_window_tests --${adapter} "${PROBE_OUTPUT}/window-${adapter}")
  set_tests_properties(font_${adapter} replacement_${adapter} material_${adapter} window_${adapter} PROPERTIES TIMEOUT 240 RUN_SERIAL TRUE)
endforeach()
