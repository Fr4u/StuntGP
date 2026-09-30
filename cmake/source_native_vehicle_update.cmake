include_guard(GLOBAL)
# R18 WP-E: the unified game's 442810 receiver. Links the native 418A90
# traversal here (source_application_runtime) to avoid the race_session cycle.
target_sources(source_application_runtime PRIVATE src/source_native_vehicle_update.cpp)
target_link_libraries(source_application_runtime PUBLIC race_session_vehicle_world)
