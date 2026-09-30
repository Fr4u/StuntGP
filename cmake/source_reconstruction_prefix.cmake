# New build wiring for the reviewed source prefixes. All targets use the
# same current source tree and project_options; no parent/prebuilt ABI archive.
include_guard(GLOBAL)
add_library(source_world_track_view STATIC src/source_world_track_view.cpp
  src/source_world_track_vertices.cpp src/source_track_vertices_x64.asm)
target_link_libraries(source_world_track_view PUBLIC world_model_draw retail_camera_transform retail_world_polygon_frame retail_world_local_camera retail_world_spatial_producer)

add_library(source_world_spatial_emission STATIC src/source_world_spatial_emission.cpp)
target_link_libraries(source_world_spatial_emission PUBLIC retail_world_polygon_frame retail_world_spatial_producer)
add_library(source_track_geometry_animation STATIC src/source_track_geometry_animation.cpp)
target_link_libraries(source_track_geometry_animation PUBLIC model_resources contact_particle_allocator retail_camera)
target_link_libraries(source_world_track_view PUBLIC source_world_spatial_emission source_track_geometry_animation)

if(TARGET race_session)
  add_library(source_track_part_initialization STATIC src/source_track_part_initialization.cpp)
  target_link_libraries(source_track_part_initialization PUBLIC race_session source_track_geometry_animation)
  add_library(source_sky_request_prefix STATIC src/source_sky_request_prefix.cpp src/source_sky_request_count.cpp src/source_sky_request_arguments.cpp)
  target_link_libraries(source_sky_request_prefix PUBLIC race_session retail_world_view_prefix)
endif()

option(SGN_BUILD_SOURCE_RECONSTRUCTION_PREFIX_RECEIVING "Build the 15 Models and 5 Sky prefix receiving cases" OFF)
if(SGN_BUILD_SOURCE_RECONSTRUCTION_PREFIX_RECEIVING)
  if(NOT TARGET source_sky_request_prefix OR NOT TARGET source_pmd_request_owner_contracts)
    message(FATAL_ERROR "Prefix receiving requires SGN_BUILD_RUNTIME and SGN_BUILD_SOURCE_PMD_REQUEST_OWNER_RECEIVING")
  endif()
  set(SGN_PREFIX_RETAIL_ROOT "${PROJECT_SOURCE_DIR}/data/retail" CACHE PATH "Actual NEW retail input root")
  add_executable(source_world_track_view_contracts tests/source_world_track_view_contracts.cpp)
  target_link_libraries(source_world_track_view_contracts PRIVATE source_world_track_view)
  add_executable(source_sky_request_prefix_contracts tests/source_sky_request_prefix_contracts.cpp)
  target_link_libraries(source_sky_request_prefix_contracts PRIVATE source_sky_request_prefix)
  add_custom_target(source_reconstruction_prefix_receiving DEPENDS
    source_world_track_view_contracts source_pmd_request_owner_contracts source_sky_request_prefix_contracts)
  foreach(case IN ITEMS cold_template_publication previous_template_before_queue_store previous_background_alias
    queue_reset_retention actual_reseed_overwrite active_template_publication raw44_distinct_from48
    part_skip_count0_and1 part_skip_null44 invalid_relocated_part invalid_outer_identity
    canonical_bank_expiry wrong_scene_binding current_matrix_alias prefix_failure_publication)
    add_test(NAME source_world_track_view_${case}
      COMMAND source_world_track_view_contracts "${SGN_PREFIX_RETAIL_ROOT}" "${case}")
  endforeach()
  foreach(case IN ITEMS completed_clear absent_sky observer_failure host_admission two_worlds)
    add_test(NAME source_sky_request_prefix_${case}
      COMMAND source_sky_request_prefix_contracts "${case}" "${SGN_PREFIX_RETAIL_ROOT}")
    set_tests_properties(source_sky_request_prefix_${case} PROPERTIES RUN_SERIAL TRUE)
  endforeach()
endif()
