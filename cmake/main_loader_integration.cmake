include_guard(GLOBAL)
# Shared implementations occur once; package fragments below own their CPP.
target_sources(source_application_literals PRIVATE
  src/source_script_path_formatter.cpp
  src/source_music_volume_literal.cpp
  src/source_window_literals.cpp
  src/source_application_crt_error.cpp
  src/source_crt_os_error.cpp
  src/source_crt_os_error_table.cpp)
include(cmake/source_version_session.cmake)
include(cmake/source_selected_script.cmake)
include(cmake/source_car_key_formatter.cmake)
include(cmake/source_rolling_demo_formatter.cmake)
include(cmake/source_script_number.cmake)
include(cmake/source_level_setting_helpers.cmake)
include(cmake/source_texture_override_literals.cmake)
include(cmake/source_model_catalog_literals.cmake)
include(cmake/source_level_settings.cmake)
include(cmake/source_level_overrides.cmake)
include(cmake/source_car_selection.cmake)
include(cmake/source_time_trial_selection.cmake)
include(cmake/source_track_replay.cmake)
include(cmake/source_cold_initialize.cmake)

target_sources(race_session PRIVATE
  src/source_level_script_load.cpp
  src/source_car_script.cpp
  src/source_script_search.cpp
  src/source_rolling_demo.cpp
  src/source_level_asset_requests.cpp)
target_link_libraries(race_session PUBLIC
  source_request_search
  source_selected_script source_car_key_formatter source_rolling_demo_formatter
  source_script_number source_level_settings source_level_overrides
  source_car_selection source_time_trial_selection source_track_replay)
# No fake SourceVehicleClear444dc9 or source entry activation is introduced.
