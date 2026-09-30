# Models overlay against checkpoint6; include once after its existing modules.
include_guard(GLOBAL)
foreach(required IN ITEMS project_options source_allocation_bank source_pmd_model retail_world_materials retail_world_material_loading retail_world_polygons scene_resources source_boot_config race_session session_world_materials font_material_d3d12)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "Models overlay requires the existing ${required} target")
  endif()
endforeach()
add_library(source_world_span STATIC src/source_world_span.cpp)
target_link_libraries(source_world_span PUBLIC source_allocation_bank)
target_link_libraries(retail_world_polygons PUBLIC source_world_span)

add_library(borrowed_material_name STATIC src/borrowed_material_name.cpp)
target_link_libraries(borrowed_material_name PUBLIC source_pmd_model)
target_link_libraries(retail_world_materials PUBLIC borrowed_material_name)

add_library(source_pmd_material_loading STATIC
  src/source_pmd_material_base.cpp
  src/source_pmd_material_loading.cpp)
target_link_libraries(source_pmd_material_loading PUBLIC
  source_pmd_model scene_resources retail_world_material_loading)
target_sources(scene_resources PRIVATE src/source_pmd_request_load.cpp)

# Core archive release must be linkable without pulling the game platform.
target_sources(source_boot_config PRIVATE src/source_archive_release.cpp)
add_library(source_material_files STATIC src/source_material_files.cpp)
target_link_libraries(source_material_files PUBLIC source_boot_config retail_world_material_loading)

# These adapters access the existing platform/world and remain in its library.
target_sources(race_session PRIVATE
  src/source_world_buffer_binding.cpp
  src/source_platform_archive_release.cpp
  src/source_polygon_vertex_clip.cpp
  src/source_polygon_indexed.cpp
  src/source_polygon_packet_pools.cpp
  src/source_screen_allocation.cpp
  src/source_spatial_light_allocation.cpp
  src/source_clip_vertex_alias.cpp
  src/source_clip_initialization.cpp)
target_link_libraries(race_session PUBLIC source_world_span)

# source_screen_base.cpp already belongs to race_session in checkpoint6.
target_sources(session_world_materials PRIVATE
  src/source_session_material_entry.cpp
  src/source_session_font_entry.cpp
  src/source_session_material_count.cpp)
target_link_libraries(session_world_materials PUBLIC
  source_pmd_material_loading source_material_files font_material_d3d12)

# Post-checkpoint7: raw five-DWORD clip packet adapter. No entry activation.
target_sources(race_session PRIVATE src/source_clip_packet_codec.cpp)

# Recovered source42E380/42E240, separate from cold-tail/warm activation.
enable_language(ASM_MASM)
add_library(source_model_resource_preparation STATIC
  src/source_model_resource_preparation.cpp src/source_model_strength_x64.asm)
target_link_libraries(source_model_resource_preparation PUBLIC
  race_session source_pmd_material_loading)

# Core-reserved next-epoch42C510; retains the existing source material owner.
target_sources(session_world_materials PRIVATE src/source_next_scene_materials.cpp)

# Next428FA0 and42E772 on the existing font/material/allocation owners.
target_sources(session_world_materials PRIVATE src/source_next_scene_fonts.cpp)

# Next42E430/427270/42E380 on the retained source bank, before camera402420.
target_sources(session_world_materials PRIVATE src/source_next_scene_storage.cpp)
target_link_libraries(session_world_materials PUBLIC source_model_resource_preparation)
