#pragma once

#include "config.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>

#include <hiredis.h>

namespace gs {
namespace redis {

// Reply 类型枚举
enum class ReplyType {
    None,
    String,
    Integer,
    Array,
    Nil,
    Status,
    Error
};

// 简化版 Redis Reply 包装器（避免直接暴露 hiredis 细节）
struct Reply {
    ReplyType   Type = ReplyType::None;
    std::string Str;
    long long   Integer = 0;
    bool        Ok = false;

    bool IsOk() const { return Type != ReplyType::Error; }
};

// Client Redis 客户端封装（当前骨架阶段仅支持单节点，集群模式后续扩展）
class Client {
public:
    Client() = default;
    explicit Client(const Config& cfg);
    ~Client();

    // 禁止拷贝，允许移动
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&& other) noexcept;
    Client& operator=(Client&& other) noexcept;

    // 连接 / 断开
    bool Connect(const Config& cfg);
    void Disconnect();
    bool IsConnected() const;

    // 心跳检测
    bool Ping();

    // 基础操作
    bool Set(const std::string& key, const std::string& value, int ttl_sec = 0);
    std::optional<std::string> Get(const std::string& key);
    bool Del(const std::vector<std::string>& keys);
    bool Del(const std::string& key);

    // 通用命令执行（高级场景）
    Reply Command(const char* fmt, ...);

    // 最后错误信息
    const std::string& LastError() const { return last_error_; }

private:
    void Swap(Client& other) noexcept;
    void Clear();

    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::string last_error_;
};

} // namespace redis
} // namespace gs
