include_guard(GLOBAL)
foreach(required IN ITEMS race_session source_application_literals)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "source_car_selection requires existing ${required}")
  endif()
endforeach()
add_library(source_car_selection STATIC
  "${PROJECT_SOURCE_DIR}/src/source_car_selection.cpp"
  "${PROJECT_SOURCE_DIR}/src/source_vehicle_clear.cpp")
target_include_directories(source_car_selection PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(source_car_selection PUBLIC race_session source_application_literals)
