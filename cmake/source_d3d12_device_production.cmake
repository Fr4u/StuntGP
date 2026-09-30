include_guard(GLOBAL)
if(NOT TARGET race_session)
 message(FATAL_ERROR "device D3D12 production requires race_session, renderer and Window/Presentation V3")
endif()
target_sources(race_session PRIVATE "${PROJECT_SOURCE_DIR}/src/source_d3d12_device_production.cpp")
#Renderer delta modifies the existing renderer target/source; do not add a second copy.
