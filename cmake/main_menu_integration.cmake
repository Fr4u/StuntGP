include_guard(GLOBAL)
enable_language(ASM_MASM)
# Existing retail_frontend_state and source_input_session keep their CPP.
# All newly recovered menu bodies share this target and the retained owners.
add_library(source_menu_reconstruction STATIC
  src/source_frontend_texture_replacement.cpp
  src/source_menu_command_consumption.cpp
  src/source_menu_command_queue.cpp
  src/source_menu_draw_children.cpp
  src/source_menu_feedback.cpp
  src/source_menu_apply_options.cpp
  src/source_menu_option_fild_x64.asm
  src/source_menu_frame.cpp
  src/source_menu_fill.cpp
  src/source_menu_fill_x64.asm
  src/source_menu_input.cpp
  src/source_menu_navigation_binding.cpp
  src/source_menu_navigation.cpp
  src/source_menu_page_draw.cpp
  src/source_menu_page_selection.cpp
  src/source_menu_text.cpp
  src/source_menu_update.cpp
  src/source_menu_widget_draw.cpp
  src/source_menu_widget_y_x64.asm
  src/source_title_draw.cpp
  src/source_title_page.cpp
  src/source_title_y_x64.asm
  src/retail_campaign_score_page.cpp
  src/retail_frontend_results.cpp)
target_link_libraries(source_menu_reconstruction PUBLIC
  project_options race_session retail_frontend_state retail_frontend_initialization
  source_frontend_packed_word
  retail_frontend_launch retail_frontend_font_metrics source_input_session
  session_outer_capture polygon_submission material_texture_replacement)
# Source input now references the recovered menu producer; preserve one CPP
# owner and let CMake express the static-library dependency explicitly.
target_link_libraries(source_input_session PUBLIC source_menu_reconstruction)
target_link_libraries(source_menu_reconstruction PUBLIC source_numeric_win64)
# An abstract page child is still an unresolved implementation requirement,
# not a reason to inject an empty override or switch the executable entry.
