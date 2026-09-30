# Literal PE menu row seeds, independent of menu execution and Session.
include_guard(GLOBAL)
add_library(retail_pause_menu_rows STATIC "${PROJECT_SOURCE_DIR}/src/retail_pause_menu_rows.cpp")
target_link_libraries(retail_pause_menu_rows PUBLIC project_options)
