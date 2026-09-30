include_guard(GLOBAL)
# Include after the menu target, catalog, parser packets and CRT float child.
foreach(required IN ITEMS source_menu_reconstruction source_application_literals source_frontend_catalog retail_campaign_data source_application_runtime)
  if(NOT TARGET ${required})
    message(FATAL_ERROR "Full frontend initialization requires ${required}")
  endif()
endforeach()
target_sources(source_application_literals PRIVATE src/source_frontend_csv_literals.cpp)
target_sources(source_menu_reconstruction PRIVATE
  src/source_frontend_csv.cpp
  src/source_frontend_csv_float.cpp
  src/source_decimal_conversion.cpp
  src/source_frontend_csv_startup.cpp
  src/source_frontend_initialization.cpp)
target_link_libraries(source_menu_reconstruction PUBLIC source_frontend_catalog retail_campaign_data)
# Existing owners retain their original CPP target membership. Parser bodies
# are supplied by their individual source packets. The Presentation float CPP
# is attached here exactly once alongside the shared decimal conversion.

# Concrete bounded atof receiver borrows the existing Native CRT implementation.
target_sources(source_application_runtime PRIVATE src/source_atof_bounded.cpp)
