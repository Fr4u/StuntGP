cmake_minimum_required(VERSION 3.25)
project(StuntGPCameraArena LANGUAGES CXX)
enable_testing()
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_library(camera_arena STATIC
 "${SGN_ROOT}/src/retail_model_matrices.cpp"
 "${SGN_ROOT}/src/retail_camera.cpp"
 "${SGN_ROOT}/src/retail_world_view_prefix.cpp"
 "${SGN_ROOT}/src/model_resources.cpp" "${SGN_ROOT}/src/source_pmd_model.cpp" "${SGN_ROOT}/src/source_allocation_bank.cpp"
 "${SGN_ROOT}/src/assets.cpp")
target_include_directories(camera_arena PUBLIC "${SGN_ROOT}/include")
if(MSVC)
 target_compile_options(camera_arena PUBLIC /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
else()
 target_compile_options(camera_arena PUBLIC -Wall -Wextra -Wpedantic -Werror -ffp-contract=off)
endif()
add_executable(camera_matrix_arena_oracle "${SGN_ROOT}/tests/camera_matrix_arena_oracle.cpp")
target_link_libraries(camera_matrix_arena_oracle PRIVATE camera_arena)
add_test(NAME camera_matrix_arena_source COMMAND camera_matrix_arena_oracle
 "${SGN_ROOT}/reports/camera-work/series5/camera-arena/arena.dat")
