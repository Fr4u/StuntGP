cmake_minimum_required(VERSION 3.25)
project(TrackPartInitializationQualification LANGUAGES CXX)
get_filename_component(SGN_ROOT "../.." ABSOLUTE)
add_executable(track_part_initialization_oracle
    "${SGN_ROOT}/tests/track_part_initialization_oracle.cpp"
    "${SGN_ROOT}/src/retail_track_part_initialization.cpp"
    "${SGN_ROOT}/src/retail_track_part_preparation.cpp"
    "${SGN_ROOT}/src/model_resources.cpp" "${SGN_ROOT}/src/source_pmd_model.cpp" "${SGN_ROOT}/src/source_allocation_bank.cpp"
    "${SGN_ROOT}/src/assets.cpp")
target_include_directories(track_part_initialization_oracle PRIVATE "${SGN_ROOT}/include")
target_compile_features(track_part_initialization_oracle PRIVATE cxx_std_23)
target_compile_options(track_part_initialization_oracle PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
