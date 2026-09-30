cmake_minimum_required(VERSION 3.25)
project(RaceHudNative LANGUAGES CXX)
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
add_executable(race_hud_native ../native.cpp
 "${SGN_ROOT}/src/retail_race_hud_submission.cpp"
 "${SGN_ROOT}/src/retail_quad_submission.cpp"
 "${SGN_ROOT}/src/retail_text_submission.cpp"
 "${SGN_ROOT}/src/retail_font_resources.cpp"
 "${SGN_ROOT}/src/source_allocation_bank.cpp"
 "${SGN_ROOT}/src/retail_screen_polygon_submission.cpp"
 "${SGN_ROOT}/src/retail_hud_layout_preparation.cpp"
 "${SGN_ROOT}/src/retail_hud_fraction.cpp"
 "${SGN_ROOT}/src/retail_audio_driver.cpp"
 "${SGN_ROOT}/src/retail_vehicle_storage.cpp"
 "${SGN_ROOT}/src/retail_game_text.cpp"
 "${SGN_ROOT}/src/retail_csv.cpp"
 "${SGN_ROOT}/src/retail_camera_vehicle.cpp"
 "${SGN_ROOT}/src/vehicle.cpp"
 "${SGN_ROOT}/src/assets.cpp")
target_include_directories(race_hud_native PRIVATE "${SGN_ROOT}/include")
target_compile_features(race_hud_native PRIVATE cxx_std_23)
target_compile_options(race_hud_native PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict /Gy)
target_link_options(race_hud_native PRIVATE /OPT:REF)
enable_testing()
if(HUD_CORPUS)
 add_test(NAME race_hud_source COMMAND race_hud_native "${HUD_CORPUS}")
endif()
