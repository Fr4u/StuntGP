include_guard(GLOBAL)
# NEW HOST (R28) sharp HUD: portable 2D texture upscaler (xBRZ-style and
# Lanczos) plus its D3D12 upload hook. Not recovered behaviour.
add_library(host_texture_upscale STATIC src/host_texture_upscale.cpp)
target_link_libraries(host_texture_upscale PUBLIC project_options)
if(NOT MSVC)
  find_package(Threads REQUIRED)
  target_link_libraries(host_texture_upscale PUBLIC Threads::Threads)
endif()
if(TARGET retail_world_material_loading)
  target_link_libraries(retail_world_material_loading PUBLIC host_texture_upscale)
endif()
if(TARGET platform_d3d12)
  target_sources(platform_d3d12 PRIVATE src/host_texture_upscale_d3d12.cpp)
  target_link_libraries(platform_d3d12 PUBLIC host_texture_upscale)
endif()
# NEW HOST (R30) load and menu performance: parallel texture processing, a
# persistent processed-texture cache and the load timeline profiler.
target_sources(host_texture_upscale PRIVATE src/host_parallel.cpp src/host_texture_disk_cache.cpp
                                            src/host_texture_processing.cpp)
foreach(consumer polygon_d3d12 host_graphics_menu)
  if(TARGET ${consumer})
    target_link_libraries(${consumer} PUBLIC host_texture_upscale)
  endif()
endforeach()
if(TARGET retail_assets)
  target_sources(retail_assets PRIVATE src/host_load_timeline.cpp)
endif()

# NEW HOST (R35) shader bytecode cache: every renderer D3DCompile goes through it.
if(TARGET polygon_d3d12)
  add_library(host_shader_cache STATIC src/host_shader_cache.cpp)
  target_link_libraries(host_shader_cache PUBLIC project_options host_texture_upscale d3dcompiler)
  target_link_libraries(polygon_d3d12 PUBLIC host_shader_cache)
endif()
