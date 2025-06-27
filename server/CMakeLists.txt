# server/CMakeLists.txt

# Find Protobuf and gRPC (they install CMake config files via Homebrew)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC       CONFIG REQUIRED)

# Build the server executable
add_executable(server
    main.cpp
)

# Tell the compiler where to look for your modules’ headers
# ${PROJECT_SOURCE_DIR} is the top-level of your repo
target_include_directories(server PRIVATE
    ${PROJECT_SOURCE_DIR}/raft     # for raft_node.h
    ${PROJECT_SOURCE_DIR}/kvstore  # for kv_store.h
    ${PROJECT_SOURCE_DIR}/rpc      # for raft.pb.h & raft.grpc.pb.h
)

# Link against all of our libraries, plus gRPC & Protobuf
target_link_libraries(server
    PRIVATE raft
    PRIVATE kvstore
    PRIVATE rpc
    PRIVATE protobuf::libprotobuf
    PRIVATE gRPC::grpc++
)

# (Optional) If you use C++17 features explicitly in server/main.cpp:
target_compile_features(server PRIVATE cxx_std_17)
