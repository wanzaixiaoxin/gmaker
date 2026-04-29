#pragma once

#include "config.hpp"
#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <utility>

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
    std::vector<Reply> Elements; // 数组类型子元素

    bool IsOk() const { return Type != ReplyType::Error; }

    // 解析为字符串（String/Status → Str, Nil → nullopt）
    std::optional<std::string> AsString() const {
        if (Type == ReplyType::String || Type == ReplyType::Status) return Str;
        if (Type == ReplyType::Nil) return std::nullopt;
        return std::nullopt;
    }

    // 解析为整数（Integer → Integer）
    std::optional<long long> AsInteger() const {
        if (Type == ReplyType::Integer) return Integer;
        return std::nullopt;
    }

    // 解析为浮点数（String → stod，用于 ZSCORE 等）
    std::optional<double> AsDouble() const {
        if (Type == ReplyType::String || Type == ReplyType::Status) {
            try { return std::stod(Str); } catch (...) { return std::nullopt; }
        }
        return std::nullopt;
    }

    // 解析为布尔值（Integer → integer != 0，用于 EXISTS/SISMEMBER/HEXISTS 等）
    std::optional<bool> AsBool() const {
        if (Type == ReplyType::Integer) return Integer != 0;
        return std::nullopt;
    }

    // 解析数组为字符串列表（用于 SMEMBERS/HKEYS/ZRANGE 等）
    std::vector<std::string> AsStringArray() const {
        std::vector<std::string> out;
        if (Type != ReplyType::Array) return out;
        out.reserve(Elements.size());
        for (const auto& e : Elements) {
            if (e.Type == ReplyType::String || e.Type == ReplyType::Status) {
                out.push_back(e.Str);
            }
        }
        return out;
    }

    // 解析数组为字符串对列表（用于 HGETALL 等）
    std::vector<std::pair<std::string, std::string>> AsStringPairs() const {
        std::vector<std::pair<std::string, std::string>> out;
        if (Type != ReplyType::Array) return out;
        for (size_t i = 0; i + 1 < Elements.size(); i += 2) {
            const auto& k = Elements[i];
            const auto& v = Elements[i + 1];
            if ((k.Type == ReplyType::String || k.Type == ReplyType::Status) &&
                (v.Type == ReplyType::String || v.Type == ReplyType::Status)) {
                out.emplace_back(k.Str, v.Str);
            }
        }
        return out;
    }

    // 解析数组为 (string, double) 对列表（用于 ZRANGE WITHSCORES 等）
    std::vector<std::pair<std::string, double>> AsStringDoublePairs() const {
        std::vector<std::pair<std::string, double>> out;
        if (Type != ReplyType::Array) return out;
        for (size_t i = 0; i + 1 < Elements.size(); i += 2) {
            const auto& m = Elements[i];
            const auto& s = Elements[i + 1];
            if ((m.Type == ReplyType::String || m.Type == ReplyType::Status) &&
                (s.Type == ReplyType::String || s.Type == ReplyType::Status)) {
                try {
                    out.emplace_back(m.Str, std::stod(s.Str));
                } catch (...) {}
            }
        }
        return out;
    }
};

// Pipeline 批量操作封装
// 通过 Client::Pipeline() 创建，追加命令后调用 Exec() 一次性获取结果
class Pipeline {
public:
    explicit Pipeline(redisContext* ctx);
    ~Pipeline() = default;

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) noexcept = default;
    Pipeline& operator=(Pipeline&&) noexcept = default;

    // ==================== String ====================
    Pipeline& Set(const std::string& key, const std::string& value, int ttl_sec = 0);
    Pipeline& SetEX(const std::string& key, const std::string& value, int ttl_sec);
    Pipeline& SetNX(const std::string& key, const std::string& value, int ttl_sec = 0);
    Pipeline& Get(const std::string& key);
    Pipeline& Del(const std::string& key);
    Pipeline& Del(const std::vector<std::string>& keys);
    Pipeline& MSet(const std::vector<std::pair<std::string, std::string>>& kvs);
    Pipeline& MGet(const std::vector<std::string>& keys);
    Pipeline& Incr(const std::string& key);
    Pipeline& Decr(const std::string& key);
    Pipeline& IncrBy(const std::string& key, long long delta);
    Pipeline& DecrBy(const std::string& key, long long delta);
    Pipeline& StrLen(const std::string& key);

    // ==================== Hash ====================
    Pipeline& HSet(const std::string& key, const std::string& field, const std::string& value);
    Pipeline& HGet(const std::string& key, const std::string& field);
    Pipeline& HMSet(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fvs);
    Pipeline& HMGet(const std::string& key, const std::vector<std::string>& fields);
    Pipeline& HGetAll(const std::string& key);
    Pipeline& HDel(const std::string& key, const std::vector<std::string>& fields);
    Pipeline& HExists(const std::string& key, const std::string& field);
    Pipeline& HLen(const std::string& key);
    Pipeline& HKeys(const std::string& key);
    Pipeline& HVals(const std::string& key);
    Pipeline& HIncrBy(const std::string& key, const std::string& field, long long delta);

    // ==================== List ====================
    Pipeline& LPush(const std::string& key, const std::vector<std::string>& values);
    Pipeline& RPush(const std::string& key, const std::vector<std::string>& values);
    Pipeline& LPop(const std::string& key);
    Pipeline& RPop(const std::string& key);
    Pipeline& LLen(const std::string& key);
    Pipeline& LRange(const std::string& key, long long start, long long stop);
    Pipeline& LIndex(const std::string& key, long long index);
    Pipeline& LTrim(const std::string& key, long long start, long long stop);
    Pipeline& LRem(const std::string& key, long long count, const std::string& value);

    // ==================== Set ====================
    Pipeline& SAdd(const std::string& key, const std::vector<std::string>& members);
    Pipeline& SRem(const std::string& key, const std::vector<std::string>& members);
    Pipeline& SMembers(const std::string& key);
    Pipeline& SIsMember(const std::string& key, const std::string& member);
    Pipeline& SCard(const std::string& key);
    Pipeline& SPop(const std::string& key);

    // ==================== Sorted Set ====================
    Pipeline& ZAdd(const std::string& key, const std::vector<std::pair<double, std::string>>& score_members);
    Pipeline& ZRem(const std::string& key, const std::vector<std::string>& members);
    Pipeline& ZRange(const std::string& key, long long start, long long stop);
    Pipeline& ZRevRange(const std::string& key, long long start, long long stop);
    Pipeline& ZRangeByScore(const std::string& key, double min, double max);
    Pipeline& ZRangeWithScores(const std::string& key, long long start, long long stop);
    Pipeline& ZRevRangeWithScores(const std::string& key, long long start, long long stop);
    Pipeline& ZRemRangeByRank(const std::string& key, long long start, long long stop);
    Pipeline& ZRemRangeByScore(const std::string& key, double min, double max);
    Pipeline& ZCard(const std::string& key);
    Pipeline& ZScore(const std::string& key, const std::string& member);
    Pipeline& ZRank(const std::string& key, const std::string& member);
    Pipeline& ZRevRank(const std::string& key, const std::string& member);
    Pipeline& ZIncrBy(const std::string& key, double increment, const std::string& member);

    // ==================== Key ====================
    Pipeline& Exists(const std::vector<std::string>& keys);
    Pipeline& Expire(const std::string& key, int ttl_sec);
    Pipeline& TTL(const std::string& key);
    Pipeline& Persist(const std::string& key);
    Pipeline& Rename(const std::string& key, const std::string& new_key);
    Pipeline& Type(const std::string& key);
    Pipeline& Keys(const std::string& pattern);

    // 执行所有追加的命令，返回与追加顺序一致的结果列表
    std::vector<Reply> Exec();
    bool Empty() const { return count_ == 0; }
    size_t Size() const { return count_; }
    void Clear();

    const std::string& LastError() const { return last_error_; }

private:
    redisContext* ctx_ = nullptr;
    std::string last_error_;
    size_t count_ = 0;

    bool AppendCommand(const char* fmt, ...);
    bool AppendCommandArgv(int argc, const char** argv, const size_t* argvlen);
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

    // ==================== String ====================
    bool Set(const std::string& key, const std::string& value, int ttl_sec = 0);
    bool SetEX(const std::string& key, const std::string& value, int ttl_sec);
    bool SetNX(const std::string& key, const std::string& value, int ttl_sec = 0);
    std::optional<std::string> Get(const std::string& key);
    bool Del(const std::vector<std::string>& keys);
    bool Del(const std::string& key);
    // MSet / MGet：成对读写，失败时 last_error_ 记录原因
    bool MSet(const std::vector<std::pair<std::string, std::string>>& kvs);
    std::vector<std::optional<std::string>> MGet(const std::vector<std::string>& keys);
    std::optional<long long> Incr(const std::string& key);
    std::optional<long long> Decr(const std::string& key);
    std::optional<long long> IncrBy(const std::string& key, long long delta);
    std::optional<long long> DecrBy(const std::string& key, long long delta);
    std::optional<long long> StrLen(const std::string& key);

    // ==================== Hash ====================
    bool HSet(const std::string& key, const std::string& field, const std::string& value);
    std::optional<std::string> HGet(const std::string& key, const std::string& field);
    bool HMSet(const std::string& key, const std::vector<std::pair<std::string, std::string>>& fvs);
    std::vector<std::optional<std::string>> HMGet(const std::string& key, const std::vector<std::string>& fields);
    std::vector<std::pair<std::string, std::string>> HGetAll(const std::string& key);
    bool HDel(const std::string& key, const std::vector<std::string>& fields);
    bool HExists(const std::string& key, const std::string& field);
    std::optional<long long> HLen(const std::string& key);
    std::vector<std::string> HKeys(const std::string& key);
    std::vector<std::string> HVals(const std::string& key);
    std::optional<long long> HIncrBy(const std::string& key, const std::string& field, long long delta);

    // ==================== List ====================
    std::optional<long long> LPush(const std::string& key, const std::vector<std::string>& values);
    std::optional<long long> RPush(const std::string& key, const std::vector<std::string>& values);
    std::optional<std::string> LPop(const std::string& key);
    std::optional<std::string> RPop(const std::string& key);
    std::optional<long long> LLen(const std::string& key);
    std::vector<std::string> LRange(const std::string& key, long long start, long long stop);
    std::optional<std::string> LIndex(const std::string& key, long long index);
    bool LTrim(const std::string& key, long long start, long long stop);
    std::optional<long long> LRem(const std::string& key, long long count, const std::string& value);

    // ==================== Set ====================
    std::optional<long long> SAdd(const std::string& key, const std::vector<std::string>& members);
    std::optional<long long> SRem(const std::string& key, const std::vector<std::string>& members);
    std::vector<std::string> SMembers(const std::string& key);
    bool SIsMember(const std::string& key, const std::string& member);
    std::optional<long long> SCard(const std::string& key);
    std::optional<std::string> SPop(const std::string& key);

    // ==================== Sorted Set ====================
    std::optional<long long> ZAdd(const std::string& key, const std::vector<std::pair<double, std::string>>& score_members);
    std::optional<long long> ZRem(const std::string& key, const std::vector<std::string>& members);
    std::vector<std::string> ZRange(const std::string& key, long long start, long long stop);
    std::vector<std::string> ZRevRange(const std::string& key, long long start, long long stop);
    std::vector<std::pair<std::string, double>> ZRangeWithScores(const std::string& key, long long start, long long stop);
    std::vector<std::pair<std::string, double>> ZRevRangeWithScores(const std::string& key, long long start, long long stop);
    std::vector<std::string> ZRangeByScore(const std::string& key, double min, double max);
    std::optional<long long> ZRemRangeByRank(const std::string& key, long long start, long long stop);
    std::optional<long long> ZRemRangeByScore(const std::string& key, double min, double max);
    std::optional<long long> ZCard(const std::string& key);
    std::optional<double> ZScore(const std::string& key, const std::string& member);
    std::optional<long long> ZRank(const std::string& key, const std::string& member);
    std::optional<long long> ZRevRank(const std::string& key, const std::string& member);
    std::optional<double> ZIncrBy(const std::string& key, double increment, const std::string& member);

    // ==================== Key ====================
    bool Exists(const std::vector<std::string>& keys);
    bool Expire(const std::string& key, int ttl_sec);
    std::optional<long long> TTL(const std::string& key);
    bool Persist(const std::string& key);
    bool Rename(const std::string& key, const std::string& new_key);
    std::optional<std::string> Type(const std::string& key);
    std::vector<std::string> Keys(const std::string& pattern);

    // ==================== Pipeline ====================
    Pipeline NewPipeline();

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
