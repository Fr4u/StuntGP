# Include after main_menu_integration.cmake defines the existing menu target.
include_guard(GLOBAL)
target_sources(source_menu_reconstruction PRIVATE src/source_main_page.cpp)
