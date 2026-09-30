cmake_minimum_required(VERSION 3.25)
project(RetailTextSubmissionQualification LANGUAGES CXX)
get_filename_component(SGN_ROOT "../.." ABSOLUTE)
add_executable(retail_text_submission_tests ../retail_text_submission_tests.cpp
 ../../src/retail_text_submission.cpp ../../src/retail_font_resources.cpp ../../src/source_allocation_bank.cpp
 ../../src/retail_screen_polygon_submission.cpp ../../src/assets.cpp)
target_include_directories(retail_text_submission_tests PRIVATE "${SGN_ROOT}/include")
target_compile_features(retail_text_submission_tests PRIVATE cxx_std_23)
target_compile_options(retail_text_submission_tests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict /Zc:__cplusplus)
enable_testing()
add_test(NAME retail_text_submission_guards COMMAND retail_text_submission_tests --guards)
