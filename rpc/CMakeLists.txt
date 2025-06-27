# rpc/CMakeLists.txt

cmake_minimum_required(VERSION 3.14)
project(rpc_stubs)

find_package(Protobuf   CONFIG REQUIRED)
find_package(gRPC        CONFIG REQUIRED)

# Grab every generated .pb.cc and .grpc.pb.cc in this folder
file(GLOB PROTO_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/*.pb.cc"
  "${CMAKE_CURRENT_SOURCE_DIR}/*.grpc.pb.cc"
)

add_library(rpc STATIC
  ${PROTO_SRCS}
)

# Let users #include "kv.pb.h", "raft.pb.h", etc.
target_include_directories(rpc PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link in the gRPC & Protobuf runtimes
target_link_libraries(rpc PUBLIC
  gRPC::grpc++
  protobuf::libprotobuf
)
