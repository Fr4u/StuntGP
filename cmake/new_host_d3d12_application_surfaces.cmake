include_guard(GLOBAL)
# NEW HOST lifecycle. Include after Presentation v1 and Window startup-v2.
foreach(required_target source_menu_presentation_backend source_application_runtime platform_d3d12 material_d3d12 renderer_loading_surfaces)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "D3D12 application lifecycle requires ${required_target}")
  endif()
endforeach()
target_link_libraries(renderer_loading_surfaces PUBLIC material_d3d12)
target_sources(source_menu_presentation_backend PRIVATE src/source_menu_presentation_lifecycle.cpp)
add_library(new_host_d3d12_application_surfaces STATIC src/new_host_d3d12_application_surfaces.cpp)
target_link_libraries(new_host_d3d12_application_surfaces PUBLIC project_options source_menu_presentation_backend
    material_d3d12 retail_frontend_state PRIVATE user32)
target_link_libraries(source_application_runtime PUBLIC new_host_d3d12_application_surfaces)
