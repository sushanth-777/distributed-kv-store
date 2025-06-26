add_executable(server
    main.cpp
)
target_link_libraries(server
    PRIVATE raft
    PRIVATE kvstore
    PRIVATE rpc
    PRIVATE gRPC::grpc++
    PRIVATE protobuf::libprotobuf
)
target_include_directories(server PUBLIC
    ${CMAKE_SOURCE_DIR}/raft
    ${CMAKE_SOURCE_DIR}/kvstore
    ${CMAKE_SOURCE_DIR}/rpc
)
