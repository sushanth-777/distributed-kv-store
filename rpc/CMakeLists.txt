find_package(Protobuf   CONFIG REQUIRED)
find_package(gRPC        CONFIG REQUIRED)

# List the generated sources explicitly:
set(RPC_SRCS
    raft.pb.cc
    raft.grpc.pb.cc
)

add_library(rpc STATIC
    ${RPC_SRCS}
)

target_include_directories(rpc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rpc PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)
