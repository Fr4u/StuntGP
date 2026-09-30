include_guard(GLOBAL)
# NEW host composition. Include after main_application_integration and
# main_frontend_material_integration. No startup or device call runs at CMake time.
foreach(required_target session_outer_frame_services retail_frontend_state retail_frontend_clock
    retail_loading_frame retail_loading_tables retail_hud_lifetime retail_lighting
    polygon_submission_capture renderer_loading_surfaces light_gamma_d3d12 source_application_runtime)
  if(NOT TARGET ${required_target})
    message(FATAL_ERROR "Menu presentation requires existing target ${required_target}")
  endif()
endforeach()
target_sources(session_outer_frame_services PRIVATE src/session_outer_frontend_frame_services.cpp)
target_link_libraries(session_outer_frame_services PUBLIC retail_frontend_state)
# NEW host scratch preparation belongs to these frame services themselves.
target_link_libraries(session_outer_frame_services PRIVATE polygon_submission_capacity)
add_library(retail_loading_update STATIC src/retail_loading_update.cpp)
target_link_libraries(retail_loading_update PUBLIC project_options retail_frontend_clock retail_hud_lifetime)
add_library(retail_loading_sequence STATIC src/retail_loading_sequence.cpp)
target_link_libraries(retail_loading_sequence PUBLIC retail_loading_update retail_loading_frame retail_lighting)
add_library(retail_loading_presentation STATIC src/retail_loading_presentation.cpp)
target_link_libraries(retail_loading_presentation PUBLIC retail_loading_sequence retail_loading_tables session_outer_frame_services)
add_library(source_menu_presentation_backend STATIC src/source_menu_presentation_backend.cpp)
target_link_libraries(source_menu_presentation_backend PUBLIC project_options session_outer_frame_services
    polygon_submission_capture renderer_loading_surfaces light_gamma_d3d12)
target_link_libraries(source_application_runtime PUBLIC retail_loading_presentation source_menu_presentation_backend)
