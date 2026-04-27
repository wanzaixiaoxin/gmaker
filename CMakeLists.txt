cmake_minimum_required(VERSION 3.16)
project(gmaker C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

# ==================== 第三方库 ====================
# ==================== libuv: 异步 I/O 库 ====================
# 优先查找本地预编译库（向后兼容），否则用 FetchContent 自动下载编译
set(UV_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rd/libuv/include)
find_library(UV_LIB uv
    PATHS ${CMAKE_CURRENT_SOURCE_DIR}/3rd/libuv/lib/win-x64
    NO_DEFAULT_PATH
)

if(UV_LIB)
    message(STATUS "Found local libuv: ${UV_LIB}")
    set(UV_TARGET ${UV_LIB})
else()
    message(STATUS "Local libuv not found, fetching via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        libuv
        GIT_REPOSITORY https://github.com/libuv/libuv.git
        GIT_TAG        v1.48.0
    )
    set(LIBUV_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(libuv)
    set(UV_TARGET uv_a)
    set(UV_INCLUDE_DIR ${libuv_SOURCE_DIR}/include)
    message(STATUS "libuv fetched and built as static target: uv_a")
endif()

include(FetchContent)

# toml11: TOML 配置解析库
FetchContent_Declare(
    toml11
    URL https://github.com/ToruNiina/toml11/archive/refs/tags/v4.2.0.tar.gz
    URL_HASH SHA256=9287971cd4a1a3992ef37e7b95a3972d1ae56410e7f8e3f300727ab1d6c79c2c
)

FetchContent_MakeAvailable(toml11)

# ==================== rapidjson: header-only JSON 库 ====================
# 优先查找本地拷贝，否则 FetchContent 自动下载
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/3rd/rapidjson/rapidjson.h)
    set(RAPIDJSON_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/3rd)
    message(STATUS "Found local rapidjson")
else()
    message(STATUS "Local rapidjson not found, fetching via FetchContent...")
    include(FetchContent)
    FetchContent_Declare(
        rapidjson
        URL https://github.com/Tencent/rapidjson/archive/refs/tags/v1.1.0.tar.gz
        URL_HASH SHA256=bf7ced29704a1e696fbccf2a2b4ea068e7774fa37f6d7dd4039d0787f8bed98e
    )
    FetchContent_GetProperties(rapidjson)
    if(NOT rapidjson_POPULATED)
        FetchContent_Populate(rapidjson)
    endif()
    set(RAPIDJSON_INCLUDE_DIR ${rapidjson_SOURCE_DIR}/include)
    message(STATUS "rapidjson fetched: ${rapidjson_SOURCE_DIR}")
endif()

# hiredis: Redis C 客户端库（本地源码构建）
set(ENABLE_SSL OFF CACHE BOOL "" FORCE)
set(DISABLE_TESTS ON CACHE BOOL "" FORCE)
add_subdirectory(3rd/hiredis)

# ==================== protobuf: 协议序列化库 ====================
# 优先级：1) 系统安装 (find_package)  2) 本地 3rd/protobuf/  3) 报错并提示安装
find_package(protobuf CONFIG QUIET)

if(protobuf_FOUND)
    message(STATUS "Found system protobuf ${protobuf_VERSION}")
    if(NOT TARGET protobuf::libprotobuf)
        message(FATAL_ERROR "System protobuf found but target protobuf::libprotobuf is missing. "
            "This may indicate a broken or partial installation.")
    endif()
    # 使用系统 protoc
    if(TARGET protobuf::protoc)
        set(CMAKE_PROTOC_EXECUTABLE $<TARGET_FILE:protobuf::protoc>)
    else()
        find_program(CMAKE_PROTOC_EXECUTABLE protoc REQUIRED)
    endif()
else()
    message(STATUS "System protobuf not found, falling back to local 3rd/protobuf/")

    set(PROTOBUF_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/3rd/protobuf/protobuf-34.1)
    set(PROTOBUF_INCLUDE_DIR ${PROTOBUF_ROOT}/src)
    set(PROTOBUF_LIB_DIR ${PROTOBUF_ROOT}/build/Release)
    set(PROTOC_EXECUTABLE ${PROTOBUF_LIB_DIR}/protoc.exe)

    find_library(PROTOBUF_LIBRARY libprotobuf
        PATHS ${PROTOBUF_LIB_DIR}
        NO_DEFAULT_PATH
    )
    if(NOT PROTOBUF_LIBRARY)
        message(FATAL_ERROR "Protobuf C++ library not found.\n"
            "Please install protobuf development files for your platform:\n"
            "  Windows (vcpkg):   vcpkg install protobuf\n"
            "                     (ensure vcpkg CMake toolchain is set)\n"
            "  Ubuntu/Debian:     sudo apt install libprotobuf-dev protobuf-compiler\n"
            "  CentOS/RHEL:       sudo yum install protobuf-devel protobuf-compiler\n"
            "  macOS:             brew install protobuf\n"
            "  Arch:              sudo pacman -S protobuf\n"
            "\n"
            "Or build locally and place in 3rd/protobuf/protobuf-34.1/:\n"
            "  1. Download https://github.com/protocolbuffers/protobuf/releases/download/v34.1/protobuf-34.1.zip\n"
            "  2. cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -Dprotobuf_BUILD_TESTS=OFF\n"
            "  3. cmake --build build --config Release")
    endif()

    find_library(UTF8_VALIDITY_LIBRARY libutf8_validity
        PATHS ${PROTOBUF_ROOT}/build/third_party/utf8_range/Release
        NO_DEFAULT_PATH
    )
    if(NOT UTF8_VALIDITY_LIBRARY)
        message(FATAL_ERROR "libutf8_validity not found in local protobuf build. "
            "Your local protobuf build may be incomplete.")
    endif()

    file(GLOB_RECURSE ABSL_LIBS
        "${PROTOBUF_ROOT}/build/_deps/absl-build/absl_*.lib"
    )
    if(NOT ABSL_LIBS)
        message(FATAL_ERROR "abseil libraries not found in ${PROTOBUF_ROOT}/build/_deps/absl-build. "
            "Your local protobuf build may be incomplete.")
    endif()

    message(STATUS "Found local libprotobuf: ${PROTOBUF_LIBRARY}")
    message(STATUS "Found local abseil libs: ${ABSL_LIBS}")

    add_library(protobuf::libprotobuf STATIC IMPORTED GLOBAL)
    set_target_properties(protobuf::libprotobuf PROPERTIES
        IMPORTED_LOCATION ${PROTOBUF_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES "${PROTOBUF_INCLUDE_DIR};${PROTOBUF_ROOT}/build/_deps/absl-src"
    )
    target_link_libraries(protobuf::libprotobuf INTERFACE
        ${UTF8_VALIDITY_LIBRARY}
        ${ABSL_LIBS}
    )

    set(CMAKE_PROTOC_EXECUTABLE ${PROTOC_EXECUTABLE})
endif()

# ==================== etcd-cpp-apiv3（可选）====================
# C++ 服务直接连接 etcd 需要此库。
# Windows 上默认关闭，因为需要 grpc/protobuf 开发包；
# Linux/Docker 上可开启（需先 apt install libgrpc++-dev protobuf-compiler-grpc）。
option(ENABLE_ETCD_DISCOVERY "Enable etcd direct discovery for C++ services" OFF)

if(ENABLE_ETCD_DISCOVERY)
    message(STATUS "etcd discovery enabled: fetching etcd-cpp-apiv3...")
    FetchContent_Declare(
        etcd-cpp-apiv3
        GIT_REPOSITORY https://github.com/etcd-cpp-apiv3/etcd-cpp-apiv3.git
        GIT_TAG        v0.15.3
    )
    # 仅编译同步核心，避免 cpprestsdk 依赖
    set(BUILD_ETCD_CORE_ONLY ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(etcd-cpp-apiv3)
    message(STATUS "etcd-cpp-apiv3 ready")
endif()

# ==================== 源文件 ====================
# libuv 异步网络封装层（已统一替代阻塞模型）
set(CPP_ASYNC_NET_SOURCES
    common/cpp/gs/net/buffer.cpp
    common/cpp/gs/net/ring_buffer.cpp
    common/cpp/gs/net/thread_pool.cpp
    common/cpp/gs/net/async/io_thread.cpp
    common/cpp/gs/net/async/event_loop.cpp
    common/cpp/gs/net/async/tcp_connection.cpp
    common/cpp/gs/net/async/tcp_listener.cpp
    common/cpp/gs/net/async/tcp_server.cpp
    common/cpp/gs/net/async/upstream.cpp
    common/cpp/gs/net/async/coalescer.cpp
    common/cpp/gs/net/async/ws_frame.cpp
    common/cpp/gs/net/async/ws_connection.cpp
    common/cpp/gs/net/async/ws_server.cpp
)

set(CPP_RPC_SOURCES
    common/cpp/gs/rpc/client.cpp
)

set(CPP_PROTO_SOURCES
    gen/cpp/registry.pb.cc
    gen/cpp/protocol.pb.cc
)

set(CPP_REGISTRY_SOURCES
    common/cpp/gs/registry/client.cpp
    common/cpp/gs/registry/upstream_manager.cpp
)

set(CPP_DISCOVERY_SOURCES
    common/cpp/gs/discovery/registry_impl.cpp
    common/cpp/gs/discovery/upstream_manager.cpp
    common/cpp/gs/discovery/factory.cpp
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

set(CPP_REDIS_SOURCES
    common/cpp/gs/redis/redis_client.cpp
)

set(CPP_REALTIME_SOURCES
    common/cpp/gs/realtime/room.cpp
    common/cpp/gs/realtime/compute_thread.cpp
    common/cpp/gs/realtime/aoi.cpp
)

# ==================== 网关服务 ====================
add_executable(gateway-cpp
    services/gateway-cpp/main.cpp
    services/gateway-cpp/gateway_config.cpp
    ${CPP_ASYNC_NET_SOURCES}
    ${CPP_RPC_SOURCES}
    ${CPP_REGISTRY_SOURCES}
    ${CPP_DISCOVERY_SOURCES}
    ${CPP_PROTO_SOURCES}
    ${CPP_CONFIG_SOURCES}
    ${CPP_CRYPTO_SOURCES}
    ${CPP_LIMITER_SOURCES}
    ${CPP_REPLAY_SOURCES}
    ${CPP_METRICS_SOURCES}
    ${CPP_REDIS_SOURCES}
)

target_include_directories(gateway-cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/cpp
    ${UV_INCLUDE_DIR}
    ${RAPIDJSON_INCLUDE_DIR}
)
target_link_libraries(gateway-cpp toml11::toml11 protobuf::libprotobuf ${UV_TARGET} hiredis)
if(ENABLE_ETCD_DISCOVERY)
    target_compile_definitions(gateway-cpp PRIVATE ENABLE_ETCD_DISCOVERY=1)
    target_link_libraries(gateway-cpp etcd-cpp-api)
endif()
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
    ${CPP_DISCOVERY_SOURCES}
    ${CPP_PROTO_SOURCES}
    ${CPP_CONFIG_SOURCES}
    ${CPP_CRYPTO_SOURCES}
    ${CPP_LIMITER_SOURCES}
    ${CPP_REPLAY_SOURCES}
    ${CPP_METRICS_SOURCES}
    ${CPP_REALTIME_SOURCES}
    ${CPP_REDIS_SOURCES}
)

target_include_directories(realtime-cpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gen/cpp
    ${UV_INCLUDE_DIR}
    ${RAPIDJSON_INCLUDE_DIR}
)
target_link_libraries(realtime-cpp toml11::toml11 protobuf::libprotobuf ${UV_TARGET} hiredis)
if(ENABLE_ETCD_DISCOVERY)
    target_compile_definitions(realtime-cpp PRIVATE ENABLE_ETCD_DISCOVERY=1)
    target_link_libraries(realtime-cpp etcd-cpp-api)
endif()
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
target_link_libraries(test-async-net ${UV_TARGET})
if(WIN32)
    target_link_libraries(test-async-net ws2_32 bcrypt iphlpapi psapi userenv dbghelp)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(test-async-net pthread dl rt)
elseif(APPLE)
    target_link_libraries(test-async-net pthread)
endif()

# ==================== Redis 客户端测试 ====================
add_executable(test-redis
    common/cpp/gs/redis/test.cpp
    common/cpp/gs/redis/redis_client.cpp
)
target_include_directories(test-redis PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
)
target_link_libraries(test-redis hiredis)
if(WIN32)
    target_link_libraries(test-redis ws2_32)
endif()

# ==================== 网络层正确性与性能测试（服务端 + 客户端分离） ====================
set(PERF_TEST_SOURCES
    ${CPP_ASYNC_NET_SOURCES}
    common/cpp/gs/crypto/aes_gcm.cpp
    common/cpp/gs/crypto/hmac.cpp
    common/cpp/gs/crypto/session.cpp
)
set(PERF_TEST_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/common/cpp
    ${UV_INCLUDE_DIR}
)
set(PERF_TEST_LIBS ${UV_TARGET})
set(PERF_TEST_WIN_LIBS ws2_32 bcrypt iphlpapi psapi userenv dbghelp)
set(PERF_TEST_UNIX_LIBS pthread dl rt)
set(PERF_TEST_APPLE_LIBS pthread)

# 服务端
add_executable(test-net-perf-server
    tests/perf/net_perf_server.cpp
    ${PERF_TEST_SOURCES}
)
target_include_directories(test-net-perf-server PRIVATE ${PERF_TEST_INCLUDES})
target_link_libraries(test-net-perf-server ${PERF_TEST_LIBS})
if(WIN32)
    target_link_libraries(test-net-perf-server ${PERF_TEST_WIN_LIBS})
elseif(UNIX AND NOT APPLE)
    target_link_libraries(test-net-perf-server ${PERF_TEST_UNIX_LIBS})
elseif(APPLE)
    target_link_libraries(test-net-perf-server ${PERF_TEST_APPLE_LIBS})
endif()

# 客户端
add_executable(test-net-perf-client
    tests/perf/net_perf_client.cpp
    ${PERF_TEST_SOURCES}
)
target_include_directories(test-net-perf-client PRIVATE ${PERF_TEST_INCLUDES})
target_link_libraries(test-net-perf-client ${PERF_TEST_LIBS})
if(WIN32)
    target_link_libraries(test-net-perf-client ${PERF_TEST_WIN_LIBS})
elseif(UNIX AND NOT APPLE)
    target_link_libraries(test-net-perf-client ${PERF_TEST_UNIX_LIBS})
elseif(APPLE)
    target_link_libraries(test-net-perf-client ${PERF_TEST_APPLE_LIBS})
endif()

# Windows: hiredis 构建为 DLL，需在构建后自动复制到可执行文件目录
if(WIN32)
    set(HIREDIS_DLL_TARGETS gateway-cpp realtime-cpp test-redis)
    foreach(tgt IN LISTS HIREDIS_DLL_TARGETS)
        if(TARGET ${tgt})
            add_custom_command(TARGET ${tgt} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "$<TARGET_FILE:hiredis>"
                    "$<TARGET_FILE_DIR:${tgt}>"
                COMMENT "Copying hiredis.dll for ${tgt}"
            )
        endif()
    endforeach()
endif()

# TODO: 后续添加 protobuf 库、gtest 等依赖
