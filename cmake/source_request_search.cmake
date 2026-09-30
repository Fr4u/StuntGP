# Recovered462890 control flow over the actual bounded owners; no native code.
include_guard(GLOBAL)
add_library(source_request_search STATIC src/source_request_search.cpp)
target_link_libraries(source_request_search PUBLIC source_application_literals)
