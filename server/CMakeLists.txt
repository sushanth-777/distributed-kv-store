# server/CMakeLists.txt

# Find Protobuf, gRPC, and Threads
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC       CONFIG REQUIRED)
find_package(Threads    REQUIRED)

# Build the server executable
add_executable(server
    main.cpp
)

# Tell the compiler where to look for headers
target_include_directories(server PRIVATE
    ${PROJECT_SOURCE_DIR}/raft      # raft_node.h
    ${PROJECT_SOURCE_DIR}/kvstore   # kv_store.h
    ${PROJECT_SOURCE_DIR}/rpc       # kv.pb.h, kv.grpc.pb.h, raft.pb.h, raft.grpc.pb.h
)

# Link against:
#  • our Raft library
#  • our in‐memory KVStore library
#  • our rpc stubs library (contains both Raft & KV gRPC code)
#  • protobuf & gRPC C++ runtimes
#  • pthreads (for std::thread, mutex, etc.)
target_link_libraries(server
    PRIVATE raft
    PRIVATE kvstore
    PRIVATE rpc
    PRIVATE protobuf::libprotobuf
    PRIVATE gRPC::grpc++
    PRIVATE Threads::Threads
)

# Use C++17
target_compile_features(server PRIVATE cxx_std_17)
