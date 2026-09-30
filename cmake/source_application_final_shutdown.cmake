include_guard(GLOBAL)
if(NOT TARGET source_application_runtime)
    message(FATAL_ERROR "final shutdown requires existing source_application_runtime")
endif()
target_sources(source_application_runtime PRIVATE
    src/source_menu_shutdown.cpp
    src/source_application_final_shutdown.cpp)
#Main must include/link Loader device-production and its shutdown successor,
#plus the Presentation terminal-detach successor. No second CRT/442180 target.
