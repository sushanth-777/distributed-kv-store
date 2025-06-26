# Find protobuf & gRPC (already in top-level CMakeLists)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC    CONFIG REQUIRED)

# If you have pre-generated sources (kv.pb.cc, kv.grpc.pb.cc), list them:
add_library(rpc STATIC
    kv.grpc.pb.cc
    kv.pb.cc            # if you committed this; otherwise generate from kv.proto
)

target_include_directories(rpc PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rpc PUBLIC
    gRPC::grpc++
    protobuf::libprotobuf
)
