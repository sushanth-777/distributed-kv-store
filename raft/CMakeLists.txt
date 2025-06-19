# Build a static library for the Raft implementation
add_library(raft STATIC
    raft_node.cpp
)

target_include_directories(raft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Link dependencies if needed (none at raft core)
# target_link_libraries(raft PUBLIC ...)