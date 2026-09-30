cmake_minimum_required(VERSION 3.25)
project(StuntGPCameraFrameValidation LANGUAGES CXX)
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(FRAME_SOURCES retail_camera_frame retail_camera_spline retail_camera_mounted retail_camera retail_camera_vehicle retail_camera_lifecycle
 retail_camera_initialization retail_camera_transform retail_vehicle_storage retail_lighting retail_light_effects
 assets vehicle retail_spline camera_path track_contact track_collision contact_particle_cast contact_sphere_cast audio_environment
 retail_replay retail_gameplay_configuration retail_game_text retail_csv retail_viewport retail_model_matrices model_resources source_pmd_model retail_trail_profiles retail_world_view_prefix
 source_boot_config source_boot_config_files source_allocation_bank)
foreach(name IN LISTS FRAME_SOURCES)
 list(APPEND FRAME_FILES "${SGN_ROOT}/src/${name}.cpp")
endforeach()
add_library(camera_frame STATIC ${FRAME_FILES})
target_include_directories(camera_frame PUBLIC "${SGN_ROOT}/include")
if(MSVC)
 target_compile_options(camera_frame PUBLIC /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
else()
 target_compile_options(camera_frame PUBLIC -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
endif()
add_executable(retail_camera_frame_tests "${SGN_ROOT}/tests/retail_camera_frame_tests.cpp")
target_link_libraries(retail_camera_frame_tests PRIVATE camera_frame)
enable_testing()
add_test(NAME retail_camera_frame_contracts COMMAND retail_camera_frame_tests
 "${SGN_ROOT}/data/retail/meshdata/track1.PMD" "${SGN_ROOT}/reports/camera-work/series2/collision/camera-collision.dat")
