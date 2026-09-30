#Include after the existing retail_game_text and (on Windows) race_session.
#The private loader-order.patch must be applied to the one existing loader.
include_guard(GLOBAL)
if(NOT TARGET retail_game_text)
  message(FATAL_ERROR "Integrate the existing retail_game_text loader before the language wrapper")
endif()
add_library(source_cold_language STATIC "${PROJECT_SOURCE_DIR}/src/source_cold_language.cpp")
target_include_directories(source_cold_language PUBLIC "${PROJECT_SOURCE_DIR}/include")
target_link_libraries(source_cold_language PUBLIC retail_game_text)

if(TARGET race_session)
  if(NOT TARGET source_boot_config)
    message(FATAL_ERROR "Source game-text file child requires existing source_boot_config")
  endif()
  add_library(source_game_text_file STATIC "${PROJECT_SOURCE_DIR}/src/source_game_text_file.cpp")
  target_link_libraries(source_game_text_file PUBLIC retail_game_text source_boot_config)
  add_library(source_cold_language_session STATIC
    "${PROJECT_SOURCE_DIR}/src/source_cold_language_session.cpp"
    "${PROJECT_SOURCE_DIR}/src/source_game_text_file_session.cpp")
  target_link_libraries(source_cold_language_session PUBLIC source_cold_language race_session source_game_text_file)
endif()

if(BUILD_TESTING)
  add_executable(source_cold_language_tests "${PROJECT_SOURCE_DIR}/tests/source_cold_language_tests.cpp")
  target_link_libraries(source_cold_language_tests PRIVATE source_cold_language)
  add_test(NAME source_cold_language_contracts COMMAND source_cold_language_tests
    "${CMAKE_CURRENT_BINARY_DIR}/source-cold-language-fixtures")
endif()

# Explicit cold tail only; no runtime activation.
include("${CMAKE_CURRENT_LIST_DIR}/source_cold_language_input.cmake")
