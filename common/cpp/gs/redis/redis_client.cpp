#include "redis_client.hpp"
#include <hiredis.h>

#ifdef _WIN32
  #include <winsock2.h> // timeval
#endif

#include <cstdarg>
#include <cstring>

namespace gs {
namespace redis {

// ==================== Impl 定义 ====================

struct Client::Impl {
    redisContext* ctx = nullptr;
};

// ==================== 构造 / 析构 / 移动 ====================

Client::Client(const Config& cfg) : impl_(std::make_unique<Impl>()) {
    Connect(cfg);
}

Client::~Client() {
    Disconnect();
}

Client::Client(Client&& other) noexcept : impl_(std::make_unique<Impl>()) {
    Swap(other);
}

Client& Client::operator=(Client&& other) noexcept {
    if (this != &other) {
        Disconnect();
        Swap(other);
    }
    return *this;
}

void Client::Swap(Client& other) noexcept {
    std::swap(impl_, other.impl_);
    std::swap(last_error_, other.last_error_);
}

void Client::Clear() {
    if (impl_ && impl_->ctx) {
        redisFree(impl_->ctx);
        impl_->ctx = nullptr;
    }
}

// ==================== 连接管理 ====================

bool Client::Connect(const Config& cfg) {
    Disconnect();
    if (!impl_) impl_ = std::make_unique<Impl>();

    if (cfg.Addrs.empty()) {
        last_error_ = "redis addrs empty";
        return false;
    }

    // 解析 host:port
    std::string host = cfg.Addrs[0];
    int port = 6379;
    auto pos = host.find(':');
    if (pos != std::string::npos) {
        port = std::stoi(host.substr(pos + 1));
        host = host.substr(0, pos);
    }

    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 0;

    impl_->ctx = redisConnectWithTimeout(host.c_str(), port, tv);
    if (!impl_->ctx || impl_->ctx->err) {
        last_error_ = impl_->ctx ? impl_->ctx->errstr : "connection failed";
        Clear();
        return false;
    }

    // 认证
    if (!cfg.Password.empty()) {
        redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "AUTH %s", cfg.Password.c_str());
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            last_error_ = reply ? reply->str : "auth failed";
            if (reply) freeReplyObject(reply);
            Clear();
            return false;
        }
        freeReplyObject(reply);
    }

    // 选库
    if (cfg.DB != 0) {
        redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "SELECT %d", cfg.DB);
        if (!reply || reply->type == REDIS_REPLY_ERROR) {
            last_error_ = reply ? reply->str : "select db failed";
            if (reply) freeReplyObject(reply);
            Clear();
            return false;
        }
        freeReplyObject(reply);
    }

    last_error_.clear();
    return true;
}

void Client::Disconnect() {
    Clear();
}

bool Client::IsConnected() const {
    return impl_ && impl_->ctx && impl_->ctx->err == 0;
}

// ==================== 基础命令 ====================

bool Client::Ping() {
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "PING");
    if (!reply) { last_error_ = "ping failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "PONG") == 0);
    if (!ok) last_error_ = reply->str ? reply->str : "ping error";
    freeReplyObject(reply);
    return ok;
}

bool Client::Set(const std::string& key, const std::string& value, int ttl_sec) {
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = nullptr;
    if (ttl_sec > 0) {
        reply = (redisReply*)redisCommand(impl_->ctx, "SET %s %s EX %d", key.c_str(), value.c_str(), ttl_sec);
    } else {
        reply = (redisReply*)redisCommand(impl_->ctx, "SET %s %s", key.c_str(), value.c_str());
    }
    if (!reply) { last_error_ = "set failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "OK") == 0);
    if (!ok) last_error_ = reply->str ? reply->str : "set error";
    freeReplyObject(reply);
    return ok;
}

std::optional<std::string> Client::Get(const std::string& key) {
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "GET %s", key.c_str());
    if (!reply) { last_error_ = "get failed"; return std::nullopt; }
    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = std::nullopt;
    } else {
        last_error_ = reply->str ? reply->str : "get error";
    }
    freeReplyObject(reply);
    return result;
}

bool Client::Del(const std::vector<std::string>& keys) {
    if (!IsConnected() || keys.empty()) { last_error_ = "not connected or empty keys"; return false; }
    std::string cmd = "DEL";
    for (const auto& k : keys) {
        cmd += " " + k;
    }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, cmd.c_str());
    if (!reply) { last_error_ = "del failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    if (!ok) last_error_ = reply->str ? reply->str : "del error";
    freeReplyObject(reply);
    return ok;
}

bool Client::Del(const std::string& key) {
    return Del(std::vector<std::string>{key});
}

// ==================== 通用命令 ====================

Reply Client::Command(const char* fmt, ...) {
    Reply r;
    if (!IsConnected()) { last_error_ = "not connected"; r.Type = ReplyType::Error; return r; }
    va_list ap;
    va_start(ap, fmt);
    redisReply* reply = (redisReply*)redisvCommand(impl_->ctx, fmt, ap);
    va_end(ap);
    if (!reply) { last_error_ = "command failed"; r.Type = ReplyType::Error; return r; }

    switch (reply->type) {
        case REDIS_REPLY_STRING:
            r.Type = ReplyType::String;
            r.Str = std::string(reply->str, reply->len);
            r.Ok = true;
            break;
        case REDIS_REPLY_INTEGER:
            r.Type = ReplyType::Integer;
            r.Integer = reply->integer;
            r.Ok = true;
            break;
        case REDIS_REPLY_ARRAY:
            r.Type = ReplyType::Array;
            r.Ok = true;
            break;
        case REDIS_REPLY_NIL:
            r.Type = ReplyType::Nil;
            r.Ok = true;
            break;
        case REDIS_REPLY_STATUS:
            r.Type = ReplyType::Status;
            r.Str = std::string(reply->str, reply->len);
            r.Ok = true;
            break;
        case REDIS_REPLY_ERROR:
            r.Type = ReplyType::Error;
            r.Str = std::string(reply->str, reply->len);
            last_error_ = r.Str;
            break;
        default:
            r.Type = ReplyType::None;
            break;
    }
    freeReplyObject(reply);
    return r;
}

} // namespace redis
} // namespace gs
