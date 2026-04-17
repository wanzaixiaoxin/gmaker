cmake_minimum_required(VERSION 3.16)
project(gmaker C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ==================== 源文件 ====================
set(CPP_NET_SOURCES
    common/cpp/gs/net/tcp_conn.cpp
    common/cpp/gs/net/tcp_server.cpp
    common/cpp/gs/net/tcp_client.cpp
)

set(CPP_RPC_SOURCES
    common/cpp/gs/rpc/client.cpp
)

set(CPP_REGISTRY_SOURCES
    common/cpp/gs/registry/client.cpp
)

# ==================== 网关服务 ====================
add_executable(gateway-cpp
    services/gateway-cpp/main.cpp
    ${CPP_NET_SOURCES}
    ${CPP_RPC_SOURCES}
    ${CPP_REGISTRY_SOURCES}
)

target_include_directories(gateway-cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/cpp
)

if(WIN32)
    target_link_libraries(gateway-cpp ws2_32)
endif()

# ==================== 实时服 ====================
add_executable(realtime-cpp
    services/realtime-cpp/main.cpp
    ${CPP_NET_SOURCES}
    ${CPP_RPC_SOURCES}
    ${CPP_REGISTRY_SOURCES}
)

target_include_directories(realtime-cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/cpp
)

if(WIN32)
    target_link_libraries(realtime-cpp ws2_32)
endif()

# TODO: 后续添加 protobuf 库、asio/libuv、gtest 等依赖
