# Build the in-memory key-value store as a static library
add_library(kvstore STATIC
    kv_store.cpp
)

# Make the headers visible to dependents
target_include_directories(kvstore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
