include_guard(GLOBAL)
# NEW HOST (R24) display modes: windowed / borderless / exclusive fullscreen
# (sgn::host_graphics::Settings::display_mode). Not recovered behaviour: host
# presentation only. The exclusive-fullscreen swap-chain work lives in
# platform_d3d12 (renderer.cpp); this library owns the window policy.
if(TARGET platform_d3d12 AND TARGET host_graphics)
  add_library(host_display_mode STATIC src/host_display_mode.cpp)
  target_link_libraries(host_display_mode PUBLIC project_options platform_d3d12 host_graphics user32)
  # renderer.cpp reads the native (preferred) display timing: QueryDisplayConfig/DisplayConfigGetDeviceInfo.
  target_link_libraries(platform_d3d12 PUBLIC user32)
  if(TARGET StuntGP_NEW)
    target_link_libraries(StuntGP_NEW PRIVATE host_display_mode)
  endif()
endif()
