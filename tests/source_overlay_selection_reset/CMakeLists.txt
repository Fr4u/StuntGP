cmake_minimum_required(VERSION 3.25)
project(SourceOverlaySelectionReset LANGUAGES CXX)
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
add_executable(source_overlay_selection_reset
    "${SGN_ROOT}/tests/source_overlay_selection_reset.cpp"
    "${SGN_ROOT}/tests/overlay_renderer_tripwire.cpp"
    "${SGN_ROOT}/src/race_overlay_textures.cpp" "${SGN_ROOT}/src/assets.cpp")
target_include_directories(source_overlay_selection_reset PRIVATE "${SGN_ROOT}/include")
target_compile_definitions(source_overlay_selection_reset PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
target_compile_options(source_overlay_selection_reset PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict /Gy)
# The unchanged overlay translation unit also references two Renderer methods.
# CPU-only link tripwires throw if either is reached; no native graphics runs.
target_link_options(source_overlay_selection_reset PRIVATE /OPT:REF)
set(SGN_RETAIL_ROOT "${SGN_ROOT}/data/retail" CACHE PATH "Verified retail AssetStore root")
enable_testing()
add_test(NAME source_overlay_selection_reset COMMAND source_overlay_selection_reset "${SGN_RETAIL_ROOT}")
