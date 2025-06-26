# Enable CTest so add_test() works
enable_testing()

# Define the RaftNode test executable
add_executable(test_raft_node
    test_raft_node.cpp
)

# Link it against your raft library
target_link_libraries(test_raft_node
    PRIVATE raft
)

# Register the test with CTest
add_test(NAME RaftNodeVoteTest COMMAND test_raft_node)
