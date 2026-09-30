# Qualified pause input and invocation wiring. These targets add no automatic
# polling, update-loop, menu callback or presentation-time behavior.
include_guard(GLOBAL)

add_library(retail_pause_input STATIC "${PROJECT_SOURCE_DIR}/src/retail_pause_input.cpp")
target_link_libraries(retail_pause_input PUBLIC retail_pause_lifecycle)

add_library(retail_pause_toggle STATIC "${PROJECT_SOURCE_DIR}/src/retail_pause_toggle.cpp")
target_link_libraries(retail_pause_toggle PUBLIC retail_pause_lifecycle)

add_library(race_session_pause STATIC "${PROJECT_SOURCE_DIR}/src/race_session_pause.cpp")
target_link_libraries(race_session_pause PUBLIC race_session retail_pause_lifecycle retail_pause_gate)
