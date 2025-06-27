cmake_minimum_required(VERSION 3.14)
project(distributed-kv-store LANGUAGES CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Protobuf and gRPC (assumes you installed via your system package manager
# or vcpkg/conan)
find_package(Protobuf CONFIG REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# Where to put generated gRPC code
set(GRPC_CPP_PLUGIN $<TARGET_FILE:gRPC::grpc_cpp_plugin>)

# Include directories for all modules
include_directories(
  ${CMAKE_CURRENT_SOURCE_DIR}/raft
  ${CMAKE_CURRENT_SOURCE_DIR}/kvstore
  ${CMAKE_CURRENT_SOURCE_DIR}/rpc
)

# Pull in each module
add_subdirectory(raft)
add_subdirectory(kvstore)
add_subdirectory(rpc)
add_subdirectory(server)
add_subdirectory(tests)
