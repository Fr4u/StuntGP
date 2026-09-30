include_guard(GLOBAL)
if(NOT TARGET source_frontend_resource_memory OR NOT TARGET retail_frontend_state)
  message(FATAL_ERROR "Ordered frontend catalog requires the existing raw resource and frontend owners")
endif()
# Shared token/decimal implementation is included by both entry adapters. This
# library does not activate the outer parent or add a runtime startup call.
add_library(source_frontend_catalog STATIC
  "${PROJECT_SOURCE_DIR}/src/source_frontend_catalog.cpp"
  "${PROJECT_SOURCE_DIR}/src/retail_frontend_language.cpp")
target_link_libraries(source_frontend_catalog PUBLIC project_options retail_frontend_state source_frontend_resource_memory)
