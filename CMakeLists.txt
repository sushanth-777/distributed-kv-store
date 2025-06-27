cmake_minimum_required(VERSION 3.14)
project(distributed-kv-store LANGUAGES CXX)

# Export compile_commands.json for editor support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Enforce C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC       CONFIG REQUIRED)
find_package(Threads    REQUIRED)

# Where to find your .proto‐generated headers
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/raft
  ${CMAKE_CURRENT_SOURCE_DIR}/kvstore
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc
)

# Build each component
add_subdirectory(raft)      # Raft consensus library
add_subdirectory(kvstore)   # In‐memory KV store
add_subdirectory(rpc)       # protobuf + gRPC stubs
add_subdirectory(server)    # Raft+KV gRPC server
add_subdirectory(tests)     # unit tests

# === KV client ===
add_executable(kv_client
  client/kv_client.cpp
)

# Make sure the client can #include "kv.pb.h" and "kv.grpc.pb.h"
target_include_directories(kv_client PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc
)

# Link in the stub library, gRPC, Protobuf, and pthreads
target_link_libraries(kv_client
  PRIVATE rpc
  PRIVATE gRPC::grpc++
  PRIVATE protobuf::libprotobuf
  PRIVATE Threads::Threads
)

# If you want to be explicit about C++17 here too
target_compile_features(kv_client PRIVATE cxx_std_17)
