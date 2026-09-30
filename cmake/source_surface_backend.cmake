# New host paired-image adapter; source/application activation remains explicit.
include_guard(GLOBAL)
foreach(required_target project_options platform_d3d12 polygon_device_context retail_loading_frame
    race_application_polygon_backend polygon_submission_capture light_gamma_d3d12 retail_material_filesystem)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Surface backend requires actual existing target ${required_target}")
  endif()
endforeach()
add_library(renderer_loading_surfaces STATIC "${PROJECT_SOURCE_DIR}/src/renderer_loading_surfaces.cpp")
target_link_libraries(renderer_loading_surfaces PUBLIC project_options platform_d3d12 polygon_device_context retail_loading_frame)
target_link_libraries(race_application_polygon_backend PUBLIC renderer_loading_surfaces)
if(BUILD_TESTING)
  add_executable(source_surface_backend_receiving "${PROJECT_SOURCE_DIR}/tests/source_surface_backend_receiving.cpp")
  target_include_directories(source_surface_backend_receiving PRIVATE "${PROJECT_SOURCE_DIR}/tests")
  target_link_libraries(source_surface_backend_receiving PRIVATE project_options renderer_loading_surfaces
    polygon_submission_capture light_gamma_d3d12 retail_material_filesystem)
  # Execution is deliberately opt-in; Main supplies adapter/retail/fresh output.
endif()
