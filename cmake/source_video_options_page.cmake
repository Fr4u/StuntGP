#Source-only PageOptionsVID kind18. Main includes after the existing menu target.
include_guard(GLOBAL)
if(NOT TARGET source_menu_reconstruction)
    message(FATAL_ERROR "source_video_options_page requires source_menu_reconstruction")
endif()
target_sources(source_menu_reconstruction PRIVATE src/source_video_options_page.cpp)
