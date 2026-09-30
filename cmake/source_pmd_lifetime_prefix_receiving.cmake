include_guard(GLOBAL)
option(SGN_BUILD_SOURCE_PMD_LIFETIME_PREFIX_RECEIVING "Build lifetime prefix CPU receiving" OFF)
if(SGN_BUILD_SOURCE_PMD_LIFETIME_PREFIX_RECEIVING)
  set(SGN_SOURCE_PMD_RETAIL_ROOT "${PROJECT_SOURCE_DIR}/data/retail" CACHE PATH "Actual retail data")
  set(SGN_LIFETIME_PREFIX_PACKET "${PROJECT_SOURCE_DIR}/reports/platform-work/series6/source-pmd-lifetime-prefix-owner-v1" CACHE PATH "Sealed receiving packet")
  set(SGN_LIFETIME_PREFIX_BASELINE "${SGN_LIFETIME_PREFIX_PACKET}/baseline" CACHE PATH "Frozen track-phase940 inputs")
  add_library(lifetime_frozen_cpu STATIC
    "${SGN_LIFETIME_PREFIX_BASELINE}/src/assets.cpp"
    "${SGN_LIFETIME_PREFIX_BASELINE}/src/source_allocation_bank.cpp"
    "${SGN_LIFETIME_PREFIX_BASELINE}/src/source_pmd_model.cpp"
    "${SGN_LIFETIME_PREFIX_BASELINE}/src/model_resources.cpp"
    "${SGN_LIFETIME_PREFIX_BASELINE}/src/vehicle_model_detail.cpp"
    "${SGN_LIFETIME_PREFIX_BASELINE}/src/vehicle_presentation.cpp"
    "${SGN_LIFETIME_PREFIX_BASELINE}/src/vehicle_model_effects.cpp"
    "${SGN_LIFETIME_PREFIX_BASELINE}/src/source_pmd_brake_lights.cpp"
    "${SGN_LIFETIME_PREFIX_BASELINE}/src/vehicle_trails.cpp"
  )
  target_include_directories(lifetime_frozen_cpu PUBLIC "${SGN_LIFETIME_PREFIX_BASELINE}/include")
  target_compile_options(lifetime_frozen_cpu PUBLIC /W4 /WX /permissive- /utf-8 /EHsc /fp:strict /Gy)
  target_link_options(lifetime_frozen_cpu PUBLIC /OPT:REF)
  target_compile_definitions(lifetime_frozen_cpu PUBLIC NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
  add_executable(source_pmd_lifetime_prefix "${PROJECT_SOURCE_DIR}/tests/source_pmd_lifetime_prefix_contracts.cpp")
  target_link_libraries(source_pmd_lifetime_prefix PRIVATE vehicle_model_detail vehicle_model_effects)
  add_executable(source_pmd_lifetime_prefix_frozen "${PROJECT_SOURCE_DIR}/tests/source_pmd_lifetime_prefix_contracts.cpp")
  target_link_libraries(source_pmd_lifetime_prefix_frozen PRIVATE lifetime_frozen_cpu)
  target_compile_definitions(source_pmd_lifetime_prefix_frozen PRIVATE SGN_LATE_REGISTRY_PREFIX=0)
  add_test(NAME lifetime_candidate_released COMMAND source_pmd_lifetime_prefix released "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_released COMMAND source_pmd_lifetime_prefix_frozen released "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_scope_released COMMAND source_pmd_lifetime_prefix scope_released "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_scope_released COMMAND source_pmd_lifetime_prefix_frozen scope_released "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_generation COMMAND source_pmd_lifetime_prefix generation "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_generation COMMAND source_pmd_lifetime_prefix_frozen generation "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_wrong_thread COMMAND source_pmd_lifetime_prefix wrong_thread "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_wrong_thread COMMAND source_pmd_lifetime_prefix_frozen wrong_thread "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_metadata_expired COMMAND source_pmd_lifetime_prefix metadata_expired "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_metadata_expired COMMAND source_pmd_lifetime_prefix_frozen metadata_expired "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_default_view COMMAND source_pmd_lifetime_prefix default_view "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_default_view COMMAND source_pmd_lifetime_prefix_frozen default_view "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_invalid_handle COMMAND source_pmd_lifetime_prefix invalid_handle "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_invalid_handle COMMAND source_pmd_lifetime_prefix_frozen invalid_handle "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_public_guard COMMAND source_pmd_lifetime_prefix public_guard "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_public_guard COMMAND source_pmd_lifetime_prefix_frozen public_guard "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_invalid_descriptor COMMAND source_pmd_lifetime_prefix invalid_descriptor "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_invalid_descriptor COMMAND source_pmd_lifetime_prefix_frozen invalid_descriptor "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_invalid_lod COMMAND source_pmd_lifetime_prefix invalid_lod "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_invalid_lod COMMAND source_pmd_lifetime_prefix_frozen invalid_lod "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_candidate_presentation_equivalence COMMAND source_pmd_lifetime_prefix presentation_equivalence "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_presentation_equivalence_RED COMMAND source_pmd_lifetime_prefix_frozen presentation_equivalence "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  set_tests_properties(lifetime_frozen_presentation_equivalence_RED PROPERTIES WILL_FAIL TRUE)
  add_test(NAME lifetime_candidate_brake_equivalence COMMAND source_pmd_lifetime_prefix brake_equivalence "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  add_test(NAME lifetime_frozen_brake_equivalence_RED COMMAND source_pmd_lifetime_prefix_frozen brake_equivalence "${SGN_SOURCE_PMD_RETAIL_ROOT}")
  set_tests_properties(lifetime_frozen_brake_equivalence_RED PROPERTIES WILL_FAIL TRUE)
endif()
