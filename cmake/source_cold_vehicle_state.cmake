# Actual40A2F0/401DE0; source numeric helpers share the existing engine40A1D0 owner.
include_guard(GLOBAL)
target_sources(source_music_runtime PRIVATE src/source_cold_vehicle_state.cpp src/source_cold_vehicle_state_x64.asm)
target_link_libraries(source_music_runtime PUBLIC source_car_selection)
