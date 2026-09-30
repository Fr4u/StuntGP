include_guard(GLOBAL)
enable_language(ASM_MASM)
# Modern build composition only; recovered routines retain their evidence citations.
add_library(source_application_runtime STATIC
  src/source_application_clock_x64.asm
  src/source_application_display_mode.cpp
  src/source_application_display_shutdown.cpp
  src/source_application_draw_start_win32.cpp
  src/source_application_entry.cpp
  src/source_application_menu_services.cpp
  src/source_application_messages_win32.cpp
  src/source_application_palette_win32.cpp
  src/source_application_shutdown.cpp
  src/source_application_surface_creation.cpp
  src/source_application_surface_registry.cpp
  src/source_application_surface_win32.cpp
  src/source_application_surfaces.cpp
  src/source_application_surfaces_late_bound.cpp
  src/source_application_window_initialize.cpp
  src/source_application_window_messages.cpp
  src/source_application_window_messages_win32.cpp
  src/source_application_window_mode.cpp
  src/source_application_window_start_win32.cpp
  src/source_application_window_start_d3d12.cpp
)
target_link_libraries(source_application_runtime PUBLIC
  project_options race_session retail_frontend_state
  source_boot_config_native source_cold_time_win32
  source_menu_reconstruction source_music_runtime source_cold_initialize
  source_version_session source_boot_config_session source_allocation_bank
  session_outer_capture polygon_submission renderer_loading_surfaces
  PRIVATE ddraw dxguid user32 gdi32)
# Build the recovered startup bodies when building the game. This does not
# switch WinMain to an incompletely composed source application world.
target_link_libraries(StuntGP_NEW PRIVATE source_application_runtime)
# Keep the composed core in the executable so its full dependency graph is
# resolved by the linker even before the native menu entry is activated.
if(MSVC)
  target_link_options(StuntGP_NEW PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:source_application_runtime>")
endif()
