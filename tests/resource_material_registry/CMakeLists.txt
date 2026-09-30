cmake_minimum_required(VERSION 3.25)
project(ResourceMaterialRegistryQualification LANGUAGES CXX)
enable_testing()
get_filename_component(SGN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
add_executable(resource_material_registry_tests "${SGN_ROOT}/tests/resource_material_registry_tests.cpp"
    "${SGN_ROOT}/src/resource_material_registry.cpp" "${SGN_ROOT}/src/retail_world_materials.cpp"
    "${SGN_ROOT}/src/model_resources.cpp" "${SGN_ROOT}/src/source_pmd_model.cpp" "${SGN_ROOT}/src/source_allocation_bank.cpp" "${SGN_ROOT}/src/assets.cpp")
target_include_directories(resource_material_registry_tests PRIVATE "${SGN_ROOT}/include")
target_compile_features(resource_material_registry_tests PRIVATE cxx_std_23)
target_compile_options(resource_material_registry_tests PRIVATE /W4 /WX /permissive- /utf-8 /EHsc /fp:strict)
add_test(NAME original_materials_actual_resources COMMAND resource_material_registry_tests
    "${SGN_ROOT}/reports/camera-work/series6/materials-v1/materials.dat" "${SGN_ROOT}")
set_tests_properties(original_materials_actual_resources PROPERTIES TIMEOUT 90)
