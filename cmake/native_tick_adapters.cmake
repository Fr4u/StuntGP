include_guard(GLOBAL)
# R18 WP-D host adapters (DESIGN A5/A6; A4 is header-only in source_transport_matrix.hpp).
# A5: in source_music_runtime (its first user, the R17 source_vehicle_replay_audio_reset.cpp, was removed
# in R26; the library already links race_session and platform_audio). WP-E's source_application_runtime
# links source_music_runtime PUBLIC, so the receiver sees it.
target_sources(source_music_runtime PRIVATE src/source_audio_device_adapter.cpp)
# A6: next to RaceSession::reset_parameters() (race_session). resolve_spline() is
# defined by WP-A (src/spline_resolution.cpp); it must be in race_session or a
# library race_session links (see HOOK.md).
target_sources(race_session PRIVATE src/source_vehicle_reset_parameters.cpp)
