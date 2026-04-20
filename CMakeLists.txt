cmake_minimum_required(VERSION 3.16)
project(gmaker C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ==================== 第三方库 ====================
# libuv: 异步 I/O 库（C++ 网络层基础）
# 使用预编译静态库 + 头文件方式引入（不再 add_subdirectory 编译源码）
set(UV_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rd/libuv/include)
find_library(UV_LIB uv
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/3rd/libuv/lib/win-x64
    NO_DEFAULT_PATH
)
if(NOT UV_LIB)
    message(FATAL_ERROR "libuv prebuilt library not found. Please build libuv and place the library in 3rd/libuv/lib/<platform>/")
endif()
message(STATUS "Found libuv: ${UV_LIB}")

include(FetchContent)

# toml11: TOML 配置解析库
FetchContent_Declare(
    toml11
    URL https://github.com/ToruNiina/toml11/archive/refs/tags/v4.2.0.tar.gz
    URL_HASH SHA256=9287971cd4a1a3992ef37e7b95a3972d1ae56410e7f8e3f300727ab1d6c79c2c
)

FetchContent_MakeAvailable(toml11)

# rapidjson: 高性能 JSON 解析/生成库（header-only，本地拷贝）
set(RAPIDJSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rd)

# ==================== 源文件 ====================
# libuv 异步网络封装层（已统一替代阻塞模型）
set(CPP_ASYNC_NET_SOURCES
    common/cpp/gs/net/async/event_loop.cpp
    common/cpp/gs/net/async/tcp_connection.cpp
    common/cpp/gs/net/async/tcp_listener.cpp
    common/cpp/gs/net/async/tcp_server.cpp
    common/cpp/gs/net/async/upstream.cpp
    common/cpp/gs/net/async/coalescer.cpp
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
    ${CPP_ASYNC_NET_SOURCES}
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
    ${UV_INCLUDE_DIR}
    ${RAPIDJSON_INCLUDE_DIR}
)
target_link_libraries(gateway-cpp toml11::toml11 ${UV_LIB})
# TODO: install protobuf and uncomment: target_link_libraries(gateway-cpp protobuf::libprotobuf)
if(WIN32)
    target_link_libraries(gateway-cpp ws2_32 bcrypt iphlpapi psapi userenv dbghelp)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(gateway-cpp pthread dl rt)
elseif(APPLE)
    target_link_libraries(gateway-cpp pthread)
endif()

# ==================== 实时服 ====================
add_executable(realtime-cpp
    services/realtime-cpp/main.cpp
    ${CPP_ASYNC_NET_SOURCES}
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
    ${UV_INCLUDE_DIR}
    ${RAPIDJSON_INCLUDE_DIR}
)
target_link_libraries(realtime-cpp toml11::toml11 ${UV_LIB})
# TODO: install protobuf and uncomment: target_link_libraries(realtime-cpp protobuf::libprotobuf)
if(WIN32)
    target_link_libraries(realtime-cpp ws2_32 bcrypt iphlpapi psapi userenv dbghelp)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(realtime-cpp pthread dl rt)
elseif(APPLE)
    target_link_libraries(realtime-cpp pthread)
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

# ==================== 异步网络封装层测试 ====================
add_executable(test-async-net
    common/cpp/gs/net/async/test.cpp
    ${CPP_ASYNC_NET_SOURCES}
    common/cpp/gs/crypto/aes_gcm.cpp
    common/cpp/gs/crypto/hmac.cpp
    common/cpp/gs/crypto/session.cpp
)
target_include_directories(test-async-net PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
)
target_include_directories(test-async-net PRIVATE ${UV_INCLUDE_DIR})
target_link_libraries(test-async-net ${UV_LIB})
if(WIN32)
    target_link_libraries(test-async-net ws2_32 bcrypt iphlpapi psapi userenv dbghelp)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(test-async-net pthread dl rt)
elseif(APPLE)
    target_link_libraries(test-async-net pthread)
endif()

# TODO: 后续添加 protobuf 库、gtest 等依赖
