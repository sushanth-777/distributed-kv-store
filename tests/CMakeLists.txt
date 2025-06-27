# Enable CTest so add_test() works
enable_testing()

# Define the RaftNode test executable
add_executable(test_raft_node
    test_raft_node.cpp
)

add_executable(test_raft_appendentries
    test_raft_appendentries.cpp
)

# Link it against your raft library
target_link_libraries(test_raft_node
    PRIVATE raft
)

target_link_libraries(test_raft_appendentries
    PRIVATE raft
)

# Register the test with CTest
add_test(NAME RaftNodeVoteTest COMMAND test_raft_node)


add_test(NAME RaftAppendEntriesTest COMMAND test_raft_appendentries)