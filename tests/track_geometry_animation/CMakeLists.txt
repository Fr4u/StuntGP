cmake_minimum_required(VERSION 3.25)
project(StuntGPTrackGeometryAnimation LANGUAGES CXX)
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(track_geometry_animation
 "${SGN_ROOT}/tests/track_geometry_animation.cpp"
 "${SGN_ROOT}/src/retail_track_geometry_animation.cpp"
 "${SGN_ROOT}/src/retail_track_part_initialization.cpp"
 "${SGN_ROOT}/src/retail_track_part_preparation.cpp"
 "${SGN_ROOT}/src/retail_track_normal_allocation.cpp"
 "${SGN_ROOT}/src/track_part_animation.cpp"
 "${SGN_ROOT}/src/contact_particle_allocator.cpp"
 "${SGN_ROOT}/src/retail_camera.cpp"
 "${SGN_ROOT}/src/model_resources.cpp" "${SGN_ROOT}/src/source_pmd_model.cpp" "${SGN_ROOT}/src/source_allocation_bank.cpp"
 "${SGN_ROOT}/src/assets.cpp")
target_include_directories(track_geometry_animation PRIVATE "${SGN_ROOT}/include")
if(MSVC)
 target_compile_options(track_geometry_animation PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
else()
 target_compile_options(track_geometry_animation PRIVATE -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
endif()
