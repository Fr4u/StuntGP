cmake_minimum_required(VERSION 3.25)
project(vehicle_presentation_full LANGUAGES CXX)
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(PRESENTATION_SOURCES retail_vehicle_presentation_frame vehicle_model_prepare vehicle_model_pose
    vehicle_model_basis vehicle_segments retail_camera model_resources source_allocation_bank source_pmd_model assets retail_world_view_prefix
    vehicle_model_detail vehicle_presentation vehicle_shadow_draw vehicle_draw_pose retail_model_draw_queue
    retail_model_matrices retail_vehicle_strip_draw retail_vehicle_strip retail_vehicle_storage
    retail_camera_vehicle vehicle_trail_render retail_trail_render retail_trail_profiles retail_trail_packets
    retail_screen_polygons retail_light_effects retail_lighting vehicle_trails world_model_draw
    vehicle_model_effects vehicle contact_player retail_race)
add_executable(vehicle_presentation_full "${SGN_ROOT}/tests/vehicle_presentation_full.cpp")
foreach(name IN LISTS PRESENTATION_SOURCES)
    target_sources(vehicle_presentation_full PRIVATE "${SGN_ROOT}/src/${name}.cpp")
endforeach()
target_include_directories(vehicle_presentation_full PRIVATE "${SGN_ROOT}/include")
target_compile_features(vehicle_presentation_full PRIVATE cxx_std_23)
target_compile_options(vehicle_presentation_full PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
enable_testing()
if(PRESENTATION_CORPUS)
    add_test(NAME full_vehicle_presentation COMMAND vehicle_presentation_full
        "${PRESENTATION_CORPUS}" "${SGN_ROOT}/data/retail")
    set_tests_properties(full_vehicle_presentation PROPERTIES TIMEOUT 90)
endif()
