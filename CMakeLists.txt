cmake_minimum_required(VERSION 3.16)
project(gmaker C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

include(FetchContent)
FetchContent_Declare(
    toml11
    URL https://github.com/ToruNiina/toml11/archive/refs/tags/v4.2.0.tar.gz
    URL_HASH SHA256=9287971cd4a1a3992ef37e7b95a3972d1ae56410e7f8e3f300727ab1d6c79c2c
)
FetchContent_MakeAvailable(toml11)

# ==================== 源文件 ====================
set(CPP_NET_SOURCES
    common/cpp/gs/net/tcp_conn.cpp
    common/cpp/gs/net/tcp_server.cpp
    common/cpp/gs/net/tcp_client.cpp
    common/cpp/gs/net/upstream.cpp
    common/cpp/gs/net/coalescer.cpp
)

set(CPP_RPC_SOURCES
    common/cpp/gs/rpc/client.cpp
)

set(CPP_PROTO_SOURCES
    gen/cpp/registry.pb.cc
)

set(CPP_REGISTRY_SOURCES
    common/cpp/gs/registry/client.cpp
)

set(CPP_CONFIG_SOURCES
    common/cpp/gs/config/config.cpp
)

set(CPP_CRYPTO_SOURCES
    common/cpp/gs/crypto/aes_gcm.cpp
    common/cpp/gs/crypto/hmac.cpp
    common/cpp/gs/crypto/session.cpp
)

set(CPP_LIMITER_SOURCES
    common/cpp/gs/limiter/token_bucket.cpp
)

set(CPP_REPLAY_SOURCES
    common/cpp/gs/replay/replay.cpp
)

set(CPP_METRICS_SOURCES
    common/cpp/gs/metrics/metrics.cpp
)

set(CPP_REALTIME_SOURCES
    common/cpp/gs/realtime/room.cpp
    common/cpp/gs/realtime/compute_thread.cpp
    common/cpp/gs/realtime/aoi.cpp
)

# ==================== 网关服务 ====================
add_executable(gateway-cpp
    services/gateway-cpp/main.cpp
    ${CPP_NET_SOURCES}
    ${CPP_RPC_SOURCES}
    ${CPP_REGISTRY_SOURCES}
    ${CPP_PROTO_SOURCES}
    ${CPP_CONFIG_SOURCES}
    ${CPP_CRYPTO_SOURCES}
    ${CPP_LIMITER_SOURCES}
    ${CPP_REPLAY_SOURCES}
    ${CPP_METRICS_SOURCES}
)

target_include_directories(gateway-cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/cpp
)

target_link_libraries(gateway-cpp toml11::toml11)
# TODO: install protobuf and uncomment: target_link_libraries(gateway-cpp protobuf::libprotobuf)
if(WIN32)
    target_link_libraries(gateway-cpp ws2_32 bcrypt)
endif()

# ==================== 实时服 ====================
add_executable(realtime-cpp
    services/realtime-cpp/main.cpp
    ${CPP_NET_SOURCES}
    ${CPP_RPC_SOURCES}
    ${CPP_REGISTRY_SOURCES}
    ${CPP_PROTO_SOURCES}
    ${CPP_CONFIG_SOURCES}
    ${CPP_CRYPTO_SOURCES}
    ${CPP_LIMITER_SOURCES}
    ${CPP_REPLAY_SOURCES}
    ${CPP_METRICS_SOURCES}
    ${CPP_REALTIME_SOURCES}
)

target_include_directories(realtime-cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/cpp
)

target_link_libraries(realtime-cpp toml11::toml11)
# TODO: install protobuf and uncomment: target_link_libraries(realtime-cpp protobuf::libprotobuf)
if(WIN32)
    target_link_libraries(realtime-cpp ws2_32 bcrypt)
endif()

# ==================== 测试可执行文件 ====================
add_executable(test-crypto
    common/cpp/gs/crypto/test.cpp
    common/cpp/gs/crypto/aes_gcm.cpp
    common/cpp/gs/crypto/hmac.cpp
)
target_include_directories(test-crypto PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
)
if(WIN32)
    target_link_libraries(test-crypto bcrypt)
endif()

# TODO: 后续添加 protobuf 库、asio/libuv、gtest 等依赖
