include_guard(GLOBAL)
if(NOT TARGET race_session)
  message(FATAL_ERROR "main page navigation requires race_session and source_main_page factory")
endif()
target_sources(race_session PRIVATE
  "${PROJECT_SOURCE_DIR}/src/source_main_page_navigation.cpp"
  "${PROJECT_SOURCE_DIR}/src/source_menu_text_navigation.cpp")
