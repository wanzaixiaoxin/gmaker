#include "redis_client.hpp"
#include <hiredis.h>

#ifdef _WIN32
  #include <winsock2.h> // timeval
#endif

#include <cstdarg>
#include <cstring>
#include <sstream>

namespace gs {
namespace redis {

// ==================== 辅助函数 ====================

static Reply ParseReply(redisReply* reply) {
    Reply r;
    if (!reply) {
        r.Type = ReplyType::Error;
        return r;
    }
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
            for (size_t i = 0; i < reply->elements; ++i) {
                r.Elements.push_back(ParseReply(reply->element[i]));
            }
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
            break;
        default:
            r.Type = ReplyType::None;
            break;
    }
    return r;
}

// 将 std::vector<std::string> 构建为 hiredis argv 格式
static void BuildArgv(const std::vector<std::string>& src,
                      std::vector<const char*>& argv,
                      std::vector<size_t>& argvlen) {
    argv.clear();
    argvlen.clear();
    argv.reserve(src.size());
    argvlen.reserve(src.size());
    for (const auto& s : src) {
        argv.push_back(s.c_str());
        argvlen.push_back(s.size());
    }
}

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
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
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
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    Clear();
}

bool Client::IsConnected() const {
    return impl_ && impl_->ctx && impl_->ctx->err == 0;
}

// ==================== 基础命令 ====================

bool Client::Ping() {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "PING");
    if (!reply) { last_error_ = "ping failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "PONG") == 0);
    if (!ok) last_error_ = reply->str ? reply->str : "ping error";
    freeReplyObject(reply);
    return ok;
}

bool Client::Set(const std::string& key, const std::string& value, int ttl_sec) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
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

bool Client::SetEX(const std::string& key, const std::string& value, int ttl_sec) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "SETEX %s %d %s", key.c_str(), ttl_sec, value.c_str());
    if (!reply) { last_error_ = "setex failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "OK") == 0);
    if (!ok) last_error_ = reply->str ? reply->str : "setex error";
    freeReplyObject(reply);
    return ok;
}

bool Client::SetNX(const std::string& key, const std::string& value, int ttl_sec) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = nullptr;
    if (ttl_sec > 0) {
        reply = (redisReply*)redisCommand(impl_->ctx, "SET %s %s NX EX %d", key.c_str(), value.c_str(), ttl_sec);
    } else {
        reply = (redisReply*)redisCommand(impl_->ctx, "SET %s %s NX", key.c_str(), value.c_str());
    }
    if (!reply) { last_error_ = "setnx failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "OK") == 0);
    if (!ok && reply->type == REDIS_REPLY_NIL) {
        // key 已存在，不算错误
        ok = false;
    } else if (!ok) {
        last_error_ = reply->str ? reply->str : "setnx error";
    }
    freeReplyObject(reply);
    return ok;
}

std::optional<std::string> Client::Get(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
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
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || keys.empty()) { last_error_ = "not connected or empty keys"; return false; }
    std::vector<std::string> args = {"DEL"};
    args.insert(args.end(), keys.begin(), keys.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "del failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    if (!ok) last_error_ = reply->str ? reply->str : "del error";
    freeReplyObject(reply);
    return ok;
}

bool Client::Del(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    return Del(std::vector<std::string>{key});
}

bool Client::MSet(const std::vector<std::pair<std::string, std::string>>& kvs) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || kvs.empty()) { last_error_ = "not connected or empty kvs"; return false; }
    std::vector<std::string> args = {"MSET"};
    for (const auto& kv : kvs) {
        args.push_back(kv.first);
        args.push_back(kv.second);
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "mset failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "OK") == 0);
    if (!ok) last_error_ = reply->str ? reply->str : "mset error";
    freeReplyObject(reply);
    return ok;
}

std::vector<std::optional<std::string>> Client::MGet(const std::vector<std::string>& keys) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || keys.empty()) { last_error_ = "not connected or empty keys"; return {}; }
    std::vector<std::string> args = {"MGET"};
    args.insert(args.end(), keys.begin(), keys.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "mget failed"; return {}; }
    std::vector<std::optional<std::string>> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        result.reserve(reply->elements);
        for (size_t i = 0; i < reply->elements; ++i) {
            redisReply* e = reply->element[i];
            if (e->type == REDIS_REPLY_STRING) {
                result.emplace_back(std::string(e->str, e->len));
            } else {
                result.emplace_back(std::nullopt);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "mget error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::Incr(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "INCR %s", key.c_str());
    if (!reply) { last_error_ = "incr failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "incr error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::Decr(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "DECR %s", key.c_str());
    if (!reply) { last_error_ = "decr failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "decr error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::IncrBy(const std::string& key, long long delta) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "INCRBY %s %lld", key.c_str(), delta);
    if (!reply) { last_error_ = "incrby failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "incrby error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::DecrBy(const std::string& key, long long delta) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "DECRBY %s %lld", key.c_str(), delta);
    if (!reply) { last_error_ = "decrby failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "decrby error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::StrLen(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "STRLEN %s", key.c_str());
    if (!reply) { last_error_ = "strlen failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "strlen error";
    }
    freeReplyObject(reply);
    return result;
}

// ==================== Hash ====================

bool Client::HSet(const std::string& key, const std::string& field, const std::string& value) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    if (!reply) { last_error_ = "hset failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && (reply->integer == 0 || reply->integer == 1));
    if (!ok) last_error_ = reply->str ? reply->str : "hset error";
    freeReplyObject(reply);
    return ok;
}

std::optional<std::string> Client::HGet(const std::string& key, const std::string& field) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "HGET %s %s", key.c_str(), field.c_str());
    if (!reply) { last_error_ = "hget failed"; return std::nullopt; }
    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = std::nullopt;
    } else {
        last_error_ = reply->str ? reply->str : "hget error";
    }
    freeReplyObject(reply);
    return result;
}

bool Client::HMSet(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fvs) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || fvs.empty()) { last_error_ = "not connected or empty fields"; return false; }
    std::vector<std::string> args = {"HMSET", key};
    for (const auto& fv : fvs) {
        args.push_back(fv.first);
        args.push_back(fv.second);
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "hmset failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "OK") == 0);
    if (!ok) last_error_ = reply->str ? reply->str : "hmset error";
    freeReplyObject(reply);
    return ok;
}

std::vector<std::optional<std::string>> Client::HMGet(const std::string& key, const std::vector<std::string>& fields) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || fields.empty()) { last_error_ = "not connected or empty fields"; return {}; }
    std::vector<std::string> args = {"HMGET", key};
    args.insert(args.end(), fields.begin(), fields.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "hmget failed"; return {}; }
    std::vector<std::optional<std::string>> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        result.reserve(reply->elements);
        for (size_t i = 0; i < reply->elements; ++i) {
            redisReply* e = reply->element[i];
            if (e->type == REDIS_REPLY_STRING) {
                result.emplace_back(std::string(e->str, e->len));
            } else {
                result.emplace_back(std::nullopt);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "hmget error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::pair<std::string, std::string>> Client::HGetAll(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "HGETALL %s", key.c_str());
    if (!reply) { last_error_ = "hgetall failed"; return {}; }
    std::vector<std::pair<std::string, std::string>> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            redisReply* k = reply->element[i];
            redisReply* v = reply->element[i + 1];
            if (k->type == REDIS_REPLY_STRING && v->type == REDIS_REPLY_STRING) {
                result.emplace_back(std::string(k->str, k->len), std::string(v->str, v->len));
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "hgetall error";
    }
    freeReplyObject(reply);
    return result;
}

bool Client::HDel(const std::string& key, const std::vector<std::string>& fields) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || fields.empty()) { last_error_ = "not connected or empty fields"; return false; }
    std::vector<std::string> args = {"HDEL", key};
    args.insert(args.end(), fields.begin(), fields.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "hdel failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer >= 0);
    if (!ok) last_error_ = reply->str ? reply->str : "hdel error";
    freeReplyObject(reply);
    return ok;
}

bool Client::HExists(const std::string& key, const std::string& field) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "HEXISTS %s %s", key.c_str(), field.c_str());
    if (!reply) { last_error_ = "hexists failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if (!ok && reply->type == REDIS_REPLY_INTEGER && reply->integer == 0) {
        // field 不存在，不算错误，返回 false
    } else if (!ok) {
        last_error_ = reply->str ? reply->str : "hexists error";
    }
    freeReplyObject(reply);
    return ok;
}

std::optional<long long> Client::HLen(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "HLEN %s", key.c_str());
    if (!reply) { last_error_ = "hlen failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "hlen error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::string> Client::HKeys(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "HKEYS %s", key.c_str());
    if (!reply) { last_error_ = "hkeys failed"; return {}; }
    std::vector<std::string> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                result.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "hkeys error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::string> Client::HVals(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "HVALS %s", key.c_str());
    if (!reply) { last_error_ = "hvals failed"; return {}; }
    std::vector<std::string> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                result.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "hvals error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::HIncrBy(const std::string& key, const std::string& field, long long delta) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "HINCRBY %s %s %lld", key.c_str(), field.c_str(), delta);
    if (!reply) { last_error_ = "hincrby failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "hincrby error";
    }
    freeReplyObject(reply);
    return result;
}

// ==================== List ====================

std::optional<long long> Client::LPush(const std::string& key, const std::vector<std::string>& values) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || values.empty()) { last_error_ = "not connected or empty values"; return std::nullopt; }
    std::vector<std::string> args = {"LPUSH", key};
    args.insert(args.end(), values.begin(), values.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "lpush failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "lpush error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::RPush(const std::string& key, const std::vector<std::string>& values) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || values.empty()) { last_error_ = "not connected or empty values"; return std::nullopt; }
    std::vector<std::string> args = {"RPUSH", key};
    args.insert(args.end(), values.begin(), values.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "rpush failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "rpush error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<std::string> Client::LPop(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "LPOP %s", key.c_str());
    if (!reply) { last_error_ = "lpop failed"; return std::nullopt; }
    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = std::nullopt;
    } else {
        last_error_ = reply->str ? reply->str : "lpop error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<std::string> Client::RPop(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "RPOP %s", key.c_str());
    if (!reply) { last_error_ = "rpop failed"; return std::nullopt; }
    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = std::nullopt;
    } else {
        last_error_ = reply->str ? reply->str : "rpop error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::LLen(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "LLEN %s", key.c_str());
    if (!reply) { last_error_ = "llen failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "llen error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::string> Client::LRange(const std::string& key, long long start, long long stop) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "LRANGE %s %lld %lld", key.c_str(), start, stop);
    if (!reply) { last_error_ = "lrange failed"; return {}; }
    std::vector<std::string> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                result.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "lrange error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<std::string> Client::LIndex(const std::string& key, long long index) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "LINDEX %s %lld", key.c_str(), index);
    if (!reply) { last_error_ = "lindex failed"; return std::nullopt; }
    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = std::nullopt;
    } else {
        last_error_ = reply->str ? reply->str : "lindex error";
    }
    freeReplyObject(reply);
    return result;
}

bool Client::LTrim(const std::string& key, long long start, long long stop) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "LTRIM %s %lld %lld", key.c_str(), start, stop);
    if (!reply) { last_error_ = "ltrim failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "OK") == 0);
    if (!ok) last_error_ = reply->str ? reply->str : "ltrim error";
    freeReplyObject(reply);
    return ok;
}

std::optional<long long> Client::LRem(const std::string& key, long long count, const std::string& value) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "LREM %s %lld %s", key.c_str(), count, value.c_str());
    if (!reply) { last_error_ = "lrem failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "lrem error";
    }
    freeReplyObject(reply);
    return result;
}

// ==================== Set ====================

std::optional<long long> Client::SAdd(const std::string& key, const std::vector<std::string>& members) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || members.empty()) { last_error_ = "not connected or empty members"; return std::nullopt; }
    std::vector<std::string> args = {"SADD", key};
    args.insert(args.end(), members.begin(), members.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "sadd failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "sadd error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::SRem(const std::string& key, const std::vector<std::string>& members) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || members.empty()) { last_error_ = "not connected or empty members"; return std::nullopt; }
    std::vector<std::string> args = {"SREM", key};
    args.insert(args.end(), members.begin(), members.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "srem failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "srem error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::string> Client::SMembers(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "SMEMBERS %s", key.c_str());
    if (!reply) { last_error_ = "smembers failed"; return {}; }
    std::vector<std::string> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                result.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "smembers error";
    }
    freeReplyObject(reply);
    return result;
}

bool Client::SIsMember(const std::string& key, const std::string& member) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "SISMEMBER %s %s", key.c_str(), member.c_str());
    if (!reply) { last_error_ = "sismember failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if (!ok && reply->type == REDIS_REPLY_INTEGER && reply->integer == 0) {
        // member 不存在，不算错误
    } else if (!ok) {
        last_error_ = reply->str ? reply->str : "sismember error";
    }
    freeReplyObject(reply);
    return ok;
}

std::optional<long long> Client::SCard(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "SCARD %s", key.c_str());
    if (!reply) { last_error_ = "scard failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "scard error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<std::string> Client::SPop(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "SPOP %s", key.c_str());
    if (!reply) { last_error_ = "spop failed"; return std::nullopt; }
    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = std::nullopt;
    } else {
        last_error_ = reply->str ? reply->str : "spop error";
    }
    freeReplyObject(reply);
    return result;
}

// ==================== Sorted Set ====================

std::optional<long long> Client::ZAdd(const std::string& key, const std::vector<std::pair<double, std::string>>& score_members) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || score_members.empty()) { last_error_ = "not connected or empty members"; return std::nullopt; }
    std::vector<std::string> args = {"ZADD", key};
    for (const auto& sm : score_members) {
        args.push_back(std::to_string(sm.first));
        args.push_back(sm.second);
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "zadd failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "zadd error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::ZRem(const std::string& key, const std::vector<std::string>& members) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || members.empty()) { last_error_ = "not connected or empty members"; return std::nullopt; }
    std::vector<std::string> args = {"ZREM", key};
    args.insert(args.end(), members.begin(), members.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "zrem failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "zrem error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::string> Client::ZRange(const std::string& key, long long start, long long stop) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZRANGE %s %lld %lld", key.c_str(), start, stop);
    if (!reply) { last_error_ = "zrange failed"; return {}; }
    std::vector<std::string> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                result.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "zrange error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::string> Client::ZRevRange(const std::string& key, long long start, long long stop) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZREVRANGE %s %lld %lld", key.c_str(), start, stop);
    if (!reply) { last_error_ = "zrevrange failed"; return {}; }
    std::vector<std::string> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                result.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "zrevrange error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::pair<std::string, double>> Client::ZRangeWithScores(const std::string& key, long long start, long long stop) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZRANGE %s %lld %lld WITHSCORES", key.c_str(), start, stop);
    if (!reply) { last_error_ = "zrange withscores failed"; return {}; }
    std::vector<std::pair<std::string, double>> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            redisReply* m = reply->element[i];
            redisReply* s = reply->element[i + 1];
            if (m->type == REDIS_REPLY_STRING && s->type == REDIS_REPLY_STRING) {
                result.emplace_back(std::string(m->str, m->len), std::stod(std::string(s->str, s->len)));
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "zrange withscores error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::pair<std::string, double>> Client::ZRevRangeWithScores(const std::string& key, long long start, long long stop) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZREVRANGE %s %lld %lld WITHSCORES", key.c_str(), start, stop);
    if (!reply) { last_error_ = "zrevrange withscores failed"; return {}; }
    std::vector<std::pair<std::string, double>> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i + 1 < reply->elements; i += 2) {
            redisReply* m = reply->element[i];
            redisReply* s = reply->element[i + 1];
            if (m->type == REDIS_REPLY_STRING && s->type == REDIS_REPLY_STRING) {
                result.emplace_back(std::string(m->str, m->len), std::stod(std::string(s->str, s->len)));
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "zrevrange withscores error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::string> Client::ZRangeByScore(const std::string& key, double min, double max) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZRANGEBYSCORE %s %f %f", key.c_str(), min, max);
    if (!reply) { last_error_ = "zrangebyscore failed"; return {}; }
    std::vector<std::string> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                result.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "zrangebyscore error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::ZRemRangeByRank(const std::string& key, long long start, long long stop) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZREMRANGEBYRANK %s %lld %lld", key.c_str(), start, stop);
    if (!reply) { last_error_ = "zremrangebyrank failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "zremrangebyrank error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::ZRemRangeByScore(const std::string& key, double min, double max) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZREMRANGEBYSCORE %s %f %f", key.c_str(), min, max);
    if (!reply) { last_error_ = "zremrangebyscore failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "zremrangebyscore error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::ZCard(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZCARD %s", key.c_str());
    if (!reply) { last_error_ = "zcard failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "zcard error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<double> Client::ZScore(const std::string& key, const std::string& member) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZSCORE %s %s", key.c_str(), member.c_str());
    if (!reply) { last_error_ = "zscore failed"; return std::nullopt; }
    std::optional<double> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::stod(std::string(reply->str, reply->len));
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = std::nullopt;
    } else {
        last_error_ = reply->str ? reply->str : "zscore error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::ZRank(const std::string& key, const std::string& member) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZRANK %s %s", key.c_str(), member.c_str());
    if (!reply) { last_error_ = "zrank failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = std::nullopt;
    } else {
        last_error_ = reply->str ? reply->str : "zrank error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<long long> Client::ZRevRank(const std::string& key, const std::string& member) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZREVRANK %s %s", key.c_str(), member.c_str());
    if (!reply) { last_error_ = "zrevrank failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = std::nullopt;
    } else {
        last_error_ = reply->str ? reply->str : "zrevrank error";
    }
    freeReplyObject(reply);
    return result;
}

std::optional<double> Client::ZIncrBy(const std::string& key, double increment, const std::string& member) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "ZINCRBY %s %f %s", key.c_str(), increment, member.c_str());
    if (!reply) { last_error_ = "zincrby failed"; return std::nullopt; }
    std::optional<double> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::stod(std::string(reply->str, reply->len));
    } else {
        last_error_ = reply->str ? reply->str : "zincrby error";
    }
    freeReplyObject(reply);
    return result;
}

// ==================== Key ====================

bool Client::Exists(const std::vector<std::string>& keys) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected() || keys.empty()) { last_error_ = "not connected or empty keys"; return false; }
    std::vector<std::string> args = {"EXISTS"};
    args.insert(args.end(), keys.begin(), keys.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    redisReply* reply = (redisReply*)redisCommandArgv(impl_->ctx, (int)argv.size(), argv.data(), argvlen.data());
    if (!reply) { last_error_ = "exists failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer > 0);
    if (!ok) last_error_ = reply->str ? reply->str : "exists error";
    freeReplyObject(reply);
    return ok;
}

bool Client::Expire(const std::string& key, int ttl_sec) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "EXPIRE %s %d", key.c_str(), ttl_sec);
    if (!reply) { last_error_ = "expire failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if (!ok && reply->type == REDIS_REPLY_INTEGER && reply->integer == 0) {
        // key 不存在
    } else if (!ok) {
        last_error_ = reply->str ? reply->str : "expire error";
    }
    freeReplyObject(reply);
    return ok;
}

std::optional<long long> Client::TTL(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "TTL %s", key.c_str());
    if (!reply) { last_error_ = "ttl failed"; return std::nullopt; }
    std::optional<long long> result;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else {
        last_error_ = reply->str ? reply->str : "ttl error";
    }
    freeReplyObject(reply);
    return result;
}

bool Client::Persist(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "PERSIST %s", key.c_str());
    if (!reply) { last_error_ = "persist failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    if (!ok && reply->type == REDIS_REPLY_INTEGER && reply->integer == 0) {
        // key 不存在或没有过期时间
    } else if (!ok) {
        last_error_ = reply->str ? reply->str : "persist error";
    }
    freeReplyObject(reply);
    return ok;
}

bool Client::Rename(const std::string& key, const std::string& new_key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return false; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "RENAME %s %s", key.c_str(), new_key.c_str());
    if (!reply) { last_error_ = "rename failed"; return false; }
    bool ok = (reply->type == REDIS_REPLY_STATUS && std::strcmp(reply->str, "OK") == 0);
    if (!ok) last_error_ = reply->str ? reply->str : "rename error";
    freeReplyObject(reply);
    return ok;
}

std::optional<std::string> Client::Type(const std::string& key) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return std::nullopt; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "TYPE %s", key.c_str());
    if (!reply) { last_error_ = "type failed"; return std::nullopt; }
    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STATUS) {
        result = std::string(reply->str, reply->len);
    } else {
        last_error_ = reply->str ? reply->str : "type error";
    }
    freeReplyObject(reply);
    return result;
}

std::vector<std::string> Client::Keys(const std::string& pattern) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    if (!IsConnected()) { last_error_ = "not connected"; return {}; }
    redisReply* reply = (redisReply*)redisCommand(impl_->ctx, "KEYS %s", pattern.c_str());
    if (!reply) { last_error_ = "keys failed"; return {}; }
    std::vector<std::string> result;
    if (reply->type == REDIS_REPLY_ARRAY) {
        for (size_t i = 0; i < reply->elements; ++i) {
            if (reply->element[i]->type == REDIS_REPLY_STRING) {
                result.emplace_back(reply->element[i]->str, reply->element[i]->len);
            }
        }
    } else {
        last_error_ = reply->str ? reply->str : "keys error";
    }
    freeReplyObject(reply);
    return result;
}

// ==================== Pipeline ====================

Pipeline Client::NewPipeline() {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    return Pipeline(impl_ ? impl_->ctx : nullptr, &impl_->mtx);
}

// ==================== 通用命令执行 ====================

Reply Client::Command(const char* fmt, ...) {
    std::lock_guard<std::recursive_mutex> lk(impl_->mtx);
    Reply r;
    if (!IsConnected()) { last_error_ = "not connected"; r.Type = ReplyType::Error; return r; }
    va_list ap;
    va_start(ap, fmt);
    redisReply* reply = (redisReply*)redisvCommand(impl_->ctx, fmt, ap);
    va_end(ap);
    if (!reply) { last_error_ = "command failed"; r.Type = ReplyType::Error; return r; }
    r = ParseReply(reply);
    if (!r.IsOk()) last_error_ = r.Str;
    freeReplyObject(reply);
    return r;
}

// ==================== Pipeline 实现 ====================

Pipeline::Pipeline(redisContext* ctx, std::recursive_mutex* mtx) : ctx_(ctx), mtx_(mtx) {}

Pipeline::Pipeline(Pipeline&& other) noexcept
    : ctx_(other.ctx_), mtx_(other.mtx_), last_error_(std::move(other.last_error_)), count_(other.count_) {
    other.ctx_ = nullptr;
    other.mtx_ = nullptr;
    other.count_ = 0;
}

Pipeline& Pipeline::operator=(Pipeline&& other) noexcept {
    if (this != &other) {
        ctx_ = other.ctx_;
        mtx_ = other.mtx_;
        last_error_ = std::move(other.last_error_);
        count_ = other.count_;
        other.ctx_ = nullptr;
        other.mtx_ = nullptr;
        other.count_ = 0;
    }
    return *this;
}

bool Pipeline::AppendCommand(const char* fmt, ...) {
    if (!ctx_) { last_error_ = "null context"; return false; }
    std::lock_guard<std::recursive_mutex> lk(*mtx_);
    va_list ap;
    va_start(ap, fmt);
    int ret = redisvAppendCommand(ctx_, fmt, ap);
    va_end(ap);
    if (ret != REDIS_OK) {
        last_error_ = ctx_->errstr ? ctx_->errstr : "append command failed";
        return false;
    }
    ++count_;
    return true;
}

bool Pipeline::AppendCommandArgv(int argc, const char** argv, const size_t* argvlen) {
    if (!ctx_) { last_error_ = "null context"; return false; }
    std::lock_guard<std::recursive_mutex> lk(*mtx_);
    int ret = redisAppendCommandArgv(ctx_, argc, argv, argvlen);
    if (ret != REDIS_OK) {
        last_error_ = ctx_->errstr ? ctx_->errstr : "append command argv failed";
        return false;
    }
    ++count_;
    return true;
}

std::vector<Reply> Pipeline::Exec() {
    std::vector<Reply> results;
    if (!ctx_ || count_ == 0) { last_error_ = "empty pipeline"; return results; }
    std::lock_guard<std::recursive_mutex> lk(*mtx_);
    results.reserve(count_);
    for (size_t i = 0; i < count_; ++i) {
        redisReply* reply = nullptr;
        if (redisGetReply(ctx_, (void**)&reply) != REDIS_OK) {
            last_error_ = ctx_->errstr ? ctx_->errstr : "get reply failed";
            results.emplace_back(ParseReply(nullptr));
            continue;
        }
        results.push_back(ParseReply(reply));
        if (reply) freeReplyObject(reply);
    }
    count_ = 0;
    return results;
}

void Pipeline::Clear() {
    if (!ctx_ || count_ == 0) return;
    std::lock_guard<std::recursive_mutex> lk(*mtx_);
    for (size_t i = 0; i < count_; ++i) {
        redisReply* reply = nullptr;
        redisGetReply(ctx_, (void**)&reply);
        if (reply) freeReplyObject(reply);
    }
    count_ = 0;
    last_error_.clear();
}

// Pipeline String
Pipeline& Pipeline::Set(const std::string& key, const std::string& value, int ttl_sec) {
    if (ttl_sec > 0) AppendCommand("SET %s %s EX %d", key.c_str(), value.c_str(), ttl_sec);
    else AppendCommand("SET %s %s", key.c_str(), value.c_str());
    return *this;
}
Pipeline& Pipeline::SetEX(const std::string& key, const std::string& value, int ttl_sec) {
    AppendCommand("SETEX %s %d %s", key.c_str(), ttl_sec, value.c_str());
    return *this;
}
Pipeline& Pipeline::SetNX(const std::string& key, const std::string& value, int ttl_sec) {
    if (ttl_sec > 0) AppendCommand("SET %s %s NX EX %d", key.c_str(), value.c_str(), ttl_sec);
    else AppendCommand("SET %s %s NX", key.c_str(), value.c_str());
    return *this;
}
Pipeline& Pipeline::Get(const std::string& key) {
    AppendCommand("GET %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::Del(const std::string& key) {
    AppendCommand("DEL %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::Del(const std::vector<std::string>& keys) {
    std::vector<std::string> args = {"DEL"};
    args.insert(args.end(), keys.begin(), keys.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::Incr(const std::string& key) {
    AppendCommand("INCR %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::Decr(const std::string& key) {
    AppendCommand("DECR %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::IncrBy(const std::string& key, long long delta) {
    AppendCommand("INCRBY %s %lld", key.c_str(), delta);
    return *this;
}
Pipeline& Pipeline::DecrBy(const std::string& key, long long delta) {
    AppendCommand("DECRBY %s %lld", key.c_str(), delta);
    return *this;
}
Pipeline& Pipeline::StrLen(const std::string& key) {
    AppendCommand("STRLEN %s", key.c_str());
    return *this;
}

// Pipeline Hash
Pipeline& Pipeline::HSet(const std::string& key, const std::string& field, const std::string& value) {
    AppendCommand("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    return *this;
}
Pipeline& Pipeline::HGet(const std::string& key, const std::string& field) {
    AppendCommand("HGET %s %s", key.c_str(), field.c_str());
    return *this;
}
Pipeline& Pipeline::HDel(const std::string& key, const std::vector<std::string>& fields) {
    std::vector<std::string> args = {"HDEL", key};
    args.insert(args.end(), fields.begin(), fields.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::HExists(const std::string& key, const std::string& field) {
    AppendCommand("HEXISTS %s %s", key.c_str(), field.c_str());
    return *this;
}
Pipeline& Pipeline::HLen(const std::string& key) {
    AppendCommand("HLEN %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::HKeys(const std::string& key) {
    AppendCommand("HKEYS %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::HVals(const std::string& key) {
    AppendCommand("HVALS %s", key.c_str());
    return *this;
}

// Pipeline List
Pipeline& Pipeline::LPush(const std::string& key, const std::vector<std::string>& values) {
    std::vector<std::string> args = {"LPUSH", key};
    args.insert(args.end(), values.begin(), values.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::RPush(const std::string& key, const std::vector<std::string>& values) {
    std::vector<std::string> args = {"RPUSH", key};
    args.insert(args.end(), values.begin(), values.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::LPop(const std::string& key) {
    AppendCommand("LPOP %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::RPop(const std::string& key) {
    AppendCommand("RPOP %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::LLen(const std::string& key) {
    AppendCommand("LLEN %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::LRange(const std::string& key, long long start, long long stop) {
    AppendCommand("LRANGE %s %lld %lld", key.c_str(), start, stop);
    return *this;
}

// Pipeline Set
Pipeline& Pipeline::SAdd(const std::string& key, const std::vector<std::string>& members) {
    std::vector<std::string> args = {"SADD", key};
    args.insert(args.end(), members.begin(), members.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::SRem(const std::string& key, const std::vector<std::string>& members) {
    std::vector<std::string> args = {"SREM", key};
    args.insert(args.end(), members.begin(), members.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::SMembers(const std::string& key) {
    AppendCommand("SMEMBERS %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::SIsMember(const std::string& key, const std::string& member) {
    AppendCommand("SISMEMBER %s %s", key.c_str(), member.c_str());
    return *this;
}
Pipeline& Pipeline::SCard(const std::string& key) {
    AppendCommand("SCARD %s", key.c_str());
    return *this;
}

// Pipeline Sorted Set
Pipeline& Pipeline::ZAdd(const std::string& key, const std::vector<std::pair<double, std::string>>& score_members) {
    std::vector<std::string> args = {"ZADD", key};
    for (const auto& sm : score_members) {
        args.push_back(std::to_string(sm.first));
        args.push_back(sm.second);
    }
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::ZRem(const std::string& key, const std::vector<std::string>& members) {
    std::vector<std::string> args = {"ZREM", key};
    args.insert(args.end(), members.begin(), members.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::ZRange(const std::string& key, long long start, long long stop) {
    AppendCommand("ZRANGE %s %lld %lld", key.c_str(), start, stop);
    return *this;
}
Pipeline& Pipeline::ZRevRange(const std::string& key, long long start, long long stop) {
    AppendCommand("ZREVRANGE %s %lld %lld", key.c_str(), start, stop);
    return *this;
}
Pipeline& Pipeline::ZRangeByScore(const std::string& key, double min, double max) {
    AppendCommand("ZRANGEBYSCORE %s %f %f", key.c_str(), min, max);
    return *this;
}
Pipeline& Pipeline::ZCard(const std::string& key) {
    AppendCommand("ZCARD %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::ZScore(const std::string& key, const std::string& member) {
    AppendCommand("ZSCORE %s %s", key.c_str(), member.c_str());
    return *this;
}
Pipeline& Pipeline::ZRank(const std::string& key, const std::string& member) {
    AppendCommand("ZRANK %s %s", key.c_str(), member.c_str());
    return *this;
}

// Pipeline Key
Pipeline& Pipeline::Exists(const std::vector<std::string>& keys) {
    std::vector<std::string> args = {"EXISTS"};
    args.insert(args.end(), keys.begin(), keys.end());
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::Expire(const std::string& key, int ttl_sec) {
    AppendCommand("EXPIRE %s %d", key.c_str(), ttl_sec);
    return *this;
}
Pipeline& Pipeline::TTL(const std::string& key) {
    AppendCommand("TTL %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::Type(const std::string& key) {
    AppendCommand("TYPE %s", key.c_str());
    return *this;
}

// Pipeline 补充方法
Pipeline& Pipeline::MSet(const std::vector<std::pair<std::string, std::string>>& kvs) {
    std::vector<std::string> args = {"MSET"};
    for (const auto& kv : kvs) { args.push_back(kv.first); args.push_back(kv.second); }
    std::vector<const char*> argv; std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::MGet(const std::vector<std::string>& keys) {
    std::vector<std::string> args = {"MGET"};
    args.insert(args.end(), keys.begin(), keys.end());
    std::vector<const char*> argv; std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::HMSet(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fvs) {
    std::vector<std::string> args = {"HMSET", key};
    for (const auto& fv : fvs) { args.push_back(fv.first); args.push_back(fv.second); }
    std::vector<const char*> argv; std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::HMGet(const std::string& key, const std::vector<std::string>& fields) {
    std::vector<std::string> args = {"HMGET", key};
    args.insert(args.end(), fields.begin(), fields.end());
    std::vector<const char*> argv; std::vector<size_t> argvlen;
    BuildArgv(args, argv, argvlen);
    AppendCommandArgv((int)argv.size(), argv.data(), argvlen.data());
    return *this;
}
Pipeline& Pipeline::HGetAll(const std::string& key) {
    AppendCommand("HGETALL %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::HIncrBy(const std::string& key, const std::string& field, long long delta) {
    AppendCommand("HINCRBY %s %s %lld", key.c_str(), field.c_str(), delta);
    return *this;
}
Pipeline& Pipeline::LIndex(const std::string& key, long long index) {
    AppendCommand("LINDEX %s %lld", key.c_str(), index);
    return *this;
}
Pipeline& Pipeline::LTrim(const std::string& key, long long start, long long stop) {
    AppendCommand("LTRIM %s %lld %lld", key.c_str(), start, stop);
    return *this;
}
Pipeline& Pipeline::LRem(const std::string& key, long long count, const std::string& value) {
    AppendCommand("LREM %s %lld %s", key.c_str(), count, value.c_str());
    return *this;
}
Pipeline& Pipeline::SPop(const std::string& key) {
    AppendCommand("SPOP %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::ZRangeWithScores(const std::string& key, long long start, long long stop) {
    AppendCommand("ZRANGE %s %lld %lld WITHSCORES", key.c_str(), start, stop);
    return *this;
}
Pipeline& Pipeline::ZRevRangeWithScores(const std::string& key, long long start, long long stop) {
    AppendCommand("ZREVRANGE %s %lld %lld WITHSCORES", key.c_str(), start, stop);
    return *this;
}
Pipeline& Pipeline::ZRemRangeByRank(const std::string& key, long long start, long long stop) {
    AppendCommand("ZREMRANGEBYRANK %s %lld %lld", key.c_str(), start, stop);
    return *this;
}
Pipeline& Pipeline::ZRemRangeByScore(const std::string& key, double min, double max) {
    AppendCommand("ZREMRANGEBYSCORE %s %f %f", key.c_str(), min, max);
    return *this;
}
Pipeline& Pipeline::ZRevRank(const std::string& key, const std::string& member) {
    AppendCommand("ZREVRANK %s %s", key.c_str(), member.c_str());
    return *this;
}
Pipeline& Pipeline::ZIncrBy(const std::string& key, double increment, const std::string& member) {
    AppendCommand("ZINCRBY %s %f %s", key.c_str(), increment, member.c_str());
    return *this;
}
Pipeline& Pipeline::Persist(const std::string& key) {
    AppendCommand("PERSIST %s", key.c_str());
    return *this;
}
Pipeline& Pipeline::Rename(const std::string& key, const std::string& new_key) {
    AppendCommand("RENAME %s %s", key.c_str(), new_key.c_str());
    return *this;
}
Pipeline& Pipeline::Keys(const std::string& pattern) {
    AppendCommand("KEYS %s", pattern.c_str());
    return *this;
}

} // namespace redis
} // namespace gs
