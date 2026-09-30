include_guard(GLOBAL)
if(NOT TARGET source_application_literals)
  message(FATAL_ERROR "Integrate source_application_literals before model catalog reads")
endif()
target_sources(source_application_literals PRIVATE
  "${PROJECT_SOURCE_DIR}/src/source_model_catalog_literals.cpp")
