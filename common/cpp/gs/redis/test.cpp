#include "redis_client.hpp"
#include <iostream>
#include <cassert>
#include <vector>

static int g_pass = 0;
static int g_fail = 0;

#define CHECK(cond, msg) do { \
    if (cond) { ++g_pass; std::cout << "  [PASS] " << msg << std::endl; } \
    else { ++g_fail; std::cerr << "  [FAIL] " << msg << std::endl; } \
} while(0)

int main() {
    std::cout << "[C++ Redis Client Test]" << std::endl;

    gs::redis::Config cfg;
    cfg.Addrs = {"192.168.0.85:6379"};
    cfg.DB = 0;

    gs::redis::Client client(cfg);
    if (!client.IsConnected()) {
        std::cout << "WARN: cannot connect to redis (expected if redis not running)" << std::endl;
        std::cout << "Error: " << client.LastError() << std::endl;
        return 0;
    }

    // 清理测试 key
    client.Del(std::vector<std::string>{
        "test:cpp:str", "test:cpp:str2", "test:cpp:str3",
        "test:cpp:hash", "test:cpp:list", "test:cpp:set",
        "test:cpp:zset", "test:cpp:key"
    });

    // ==================== String ====================
    std::cout << "\n[String]" << std::endl;

    CHECK(client.Set("test:cpp:str", "hello", 60), "Set basic");
    auto val = client.Get("test:cpp:str");
    CHECK(val && *val == "hello", "Get basic");

    CHECK(client.SetNX("test:cpp:str2", "world"), "SetNX new key");
    CHECK(!client.SetNX("test:cpp:str2", "world2"), "SetNX existing key returns false");

    CHECK(client.SetEX("test:cpp:str3", "expire_test", 10), "SetEX");

    // MSet / MGet
    CHECK(client.MSet({{"test:cpp:m1", "v1"}, {"test:cpp:m2", "v2"}}), "MSet");
    auto mvals = client.MGet({"test:cpp:m1", "test:cpp:m2", "test:cpp:nonexist"});
    CHECK(mvals.size() == 3 && mvals[0] && *mvals[0] == "v1" && mvals[1] && *mvals[1] == "v2" && !mvals[2], "MGet");
    client.Del(std::vector<std::string>{"test:cpp:m1", "test:cpp:m2"});

    // Incr / Decr
    client.Set("test:cpp:counter", "10");
    auto incr = client.Incr("test:cpp:counter");
    CHECK(incr && *incr == 11, "Incr");
    auto decr = client.Decr("test:cpp:counter");
    CHECK(decr && *decr == 10, "Decr");
    auto incrby = client.IncrBy("test:cpp:counter", 5);
    CHECK(incrby && *incrby == 15, "IncrBy");
    auto decrby = client.DecrBy("test:cpp:counter", 3);
    CHECK(decrby && *decrby == 12, "DecrBy");
    auto slen = client.StrLen("test:cpp:counter");
    CHECK(slen && *slen == 2, "StrLen");
    client.Del(std::vector<std::string>{"test:cpp:counter"});

    // ==================== Hash ====================
    std::cout << "\n[Hash]" << std::endl;

    CHECK(client.HSet("test:cpp:hash", "name", "alice"), "HSet");
    CHECK(client.HSet("test:cpp:hash", "age", "30"), "HSet second field");
    auto hval = client.HGet("test:cpp:hash", "name");
    CHECK(hval && *hval == "alice", "HGet");
    CHECK(client.HExists("test:cpp:hash", "name"), "HExists true");
    CHECK(!client.HExists("test:cpp:hash", "nonexist"), "HExists false");

    CHECK(client.HMSet("test:cpp:hash", {{"city", "beijing"}, {"job", "dev"}}), "HMSet");
    auto hmvals = client.HMGet("test:cpp:hash", {"name", "city", "nonexist"});
    CHECK(hmvals.size() == 3 && hmvals[0] && *hmvals[0] == "alice" && hmvals[1] && *hmvals[1] == "beijing" && !hmvals[2], "HMGet");

    auto hall = client.HGetAll("test:cpp:hash");
    CHECK(!hall.empty(), "HGetAll not empty");

    auto hlen = client.HLen("test:cpp:hash");
    CHECK(hlen && *hlen >= 4, "HLen");

    auto hkeys = client.HKeys("test:cpp:hash");
    CHECK(!hkeys.empty(), "HKeys not empty");
    auto hvals = client.HVals("test:cpp:hash");
    CHECK(!hvals.empty(), "HVals not empty");

    CHECK(client.HDel("test:cpp:hash", {"job"}), "HDel");
    CHECK(!client.HExists("test:cpp:hash", "job"), "HDel verified");

    client.HSet("test:cpp:hash", "score", "100");
    auto hincr = client.HIncrBy("test:cpp:hash", "score", 10);
    CHECK(hincr && *hincr == 110, "HIncrBy");

    // ==================== List ====================
    std::cout << "\n[List]" << std::endl;

    auto lpush = client.LPush("test:cpp:list", {"a", "b", "c"});
    CHECK(lpush && *lpush == 3, "LPush");
    auto rpush = client.RPush("test:cpp:list", {"x", "y"});
    CHECK(rpush && *rpush == 5, "RPush");

    auto llen = client.LLen("test:cpp:list");
    CHECK(llen && *llen == 5, "LLen");

    auto lrange = client.LRange("test:cpp:list", 0, -1);
    CHECK(lrange.size() == 5, "LRange size");

    auto lpop = client.LPop("test:cpp:list");
    CHECK(lpop && *lpop == "c", "LPop");
    auto rpop = client.RPop("test:cpp:list");
    CHECK(rpop && *rpop == "y", "RPop");

    auto lindex = client.LIndex("test:cpp:list", 0);
    CHECK(lindex && *lindex == "b", "LIndex");

    CHECK(client.LTrim("test:cpp:list", 0, 1), "LTrim");
    auto llen2 = client.LLen("test:cpp:list");
    CHECK(llen2 && *llen2 == 2, "LTrim verified");

    client.RPush("test:cpp:list", {"a"});
    auto lrem = client.LRem("test:cpp:list", 1, "a");
    CHECK(lrem && *lrem == 1, "LRem");

    // ==================== Set ====================
    std::cout << "\n[Set]" << std::endl;

    auto sadd = client.SAdd("test:cpp:set", {"one", "two", "three"});
    CHECK(sadd && *sadd == 3, "SAdd");
    auto sadd2 = client.SAdd("test:cpp:set", {"one", "four"});
    CHECK(sadd2 && *sadd2 == 1, "SAdd partial");

    auto smembers = client.SMembers("test:cpp:set");
    CHECK(smembers.size() == 4, "SMembers size");
    CHECK(client.SIsMember("test:cpp:set", "one"), "SIsMember true");
    CHECK(!client.SIsMember("test:cpp:set", "five"), "SIsMember false");

    auto scard = client.SCard("test:cpp:set");
    CHECK(scard && *scard == 4, "SCard");

    auto spop = client.SPop("test:cpp:set");
    CHECK(spop.has_value(), "SPop");
    auto scard2 = client.SCard("test:cpp:set");
    CHECK(scard2 && *scard2 == 3, "SCard after SPop");

    auto srem = client.SRem("test:cpp:set", {"one", "two"});
    CHECK(srem && *srem >= 0, "SRem");

    // ==================== Sorted Set ====================
    std::cout << "\n[Sorted Set]" << std::endl;

    auto zadd = client.ZAdd("test:cpp:zset", {{1.0, "a"}, {2.0, "b"}, {3.0, "c"}});
    CHECK(zadd && *zadd == 3, "ZAdd");

    auto zrange = client.ZRange("test:cpp:zset", 0, -1);
    CHECK(zrange.size() == 3 && zrange[0] == "a" && zrange[1] == "b" && zrange[2] == "c", "ZRange");

    auto zrevrange = client.ZRevRange("test:cpp:zset", 0, -1);
    CHECK(zrevrange.size() == 3 && zrevrange[0] == "c" && zrevrange[1] == "b" && zrevrange[2] == "a", "ZRevRange");

    auto zrangews = client.ZRangeWithScores("test:cpp:zset", 0, -1);
    CHECK(zrangews.size() == 3 && zrangews[0].first == "a" && zrangews[0].second == 1.0, "ZRangeWithScores");

    auto zscore = client.ZScore("test:cpp:zset", "b");
    CHECK(zscore && *zscore == 2.0, "ZScore");

    auto zrank = client.ZRank("test:cpp:zset", "b");
    CHECK(zrank && *zrank == 1, "ZRank");
    auto zrevrank = client.ZRevRank("test:cpp:zset", "b");
    CHECK(zrevrank && *zrevrank == 1, "ZRevRank");

    auto zincr = client.ZIncrBy("test:cpp:zset", 5.0, "a");
    CHECK(zincr && *zincr == 6.0, "ZIncrBy");

    auto zcard = client.ZCard("test:cpp:zset");
    CHECK(zcard && *zcard == 3, "ZCard");

    auto zrem = client.ZRem("test:cpp:zset", {"a"});
    CHECK(zrem && *zrem == 1, "ZRem");

    client.ZAdd("test:cpp:zset", {{10.0, "d"}, {20.0, "e"}});
    auto zrangebs = client.ZRangeByScore("test:cpp:zset", 2.0, 15.0);
    CHECK(zrangebs.size() == 3, "ZRangeByScore");

    auto zremrr = client.ZRemRangeByRank("test:cpp:zset", 0, 0);
    CHECK(zremrr && *zremrr == 1, "ZRemRangeByRank");

    // ==================== Key ====================
    std::cout << "\n[Key]" << std::endl;

    client.Set("test:cpp:key", "val");
    CHECK(client.Exists({"test:cpp:key"}), "Exists true");
    CHECK(!client.Exists({"test:cpp:nonexist_key"}), "Exists false");

    CHECK(client.Expire("test:cpp:key", 3600), "Expire");
    auto ttl = client.TTL("test:cpp:key");
    CHECK(ttl && *ttl > 0, "TTL positive");

    CHECK(client.Persist("test:cpp:key"), "Persist");
    auto ttl2 = client.TTL("test:cpp:key");
    CHECK(ttl2 && *ttl2 == -1, "TTL after persist");

    CHECK(client.Rename("test:cpp:key", "test:cpp:key2"), "Rename");
    CHECK(!client.Exists({"test:cpp:key"}) && client.Exists({"test:cpp:key2"}), "Rename verified");

    auto type = client.Type("test:cpp:key2");
    CHECK(type && *type == "string", "Type");

    auto keys = client.Keys("test:cpp:key2");
    CHECK(!keys.empty(), "Keys");

    client.Del("test:cpp:key2");

    // ==================== Pipeline ====================
    std::cout << "\n[Pipeline]" << std::endl;

    auto pipe = client.NewPipeline();
    pipe.Set("test:cpp:pipe1", "v1")
        .Get("test:cpp:pipe1")
        .Incr("test:cpp:pipe_counter")
        .HSet("test:cpp:pipe_hash", "f1", "h1")
        .HGet("test:cpp:pipe_hash", "f1")
        .LPush("test:cpp:pipe_list", {"l1"})
        .SAdd("test:cpp:pipe_set", {"s1"})
        .ZAdd("test:cpp:pipe_zset", {{1.0, "z1"}})
        .Del({"test:cpp:pipe1", "test:cpp:pipe_hash", "test:cpp:pipe_list", "test:cpp:pipe_set", "test:cpp:pipe_zset"});

    CHECK(pipe.Size() == 9, "Pipeline size");
    auto replies = pipe.Exec();
    CHECK(replies.size() == 9, "Pipeline exec size");
    CHECK(replies[0].IsOk(), "Pipeline Set OK");
    CHECK(replies[1].Type == gs::redis::ReplyType::String && replies[1].Str == "v1", "Pipeline Get OK");
    CHECK(replies[2].Type == gs::redis::ReplyType::Integer, "Pipeline Incr OK");
    CHECK(replies[3].IsOk(), "Pipeline HSet OK");
    CHECK(replies[4].Type == gs::redis::ReplyType::String && replies[4].Str == "h1", "Pipeline HGet OK");
    CHECK(replies[5].IsOk(), "Pipeline LPush OK");
    CHECK(replies[6].IsOk(), "Pipeline SAdd OK");
    CHECK(replies[7].IsOk(), "Pipeline ZAdd OK");
    CHECK(replies[8].IsOk(), "Pipeline Del OK");

    // 清理
    client.Del(std::vector<std::string>{
        "test:cpp:str", "test:cpp:str2", "test:cpp:str3",
        "test:cpp:hash", "test:cpp:list", "test:cpp:set",
        "test:cpp:zset", "test:cpp:key", "test:cpp:key2",
        "test:cpp:pipe_counter"
    });

    // ==================== 通用命令 ====================
    std::cout << "\n[Command]" << std::endl;
    client.Set("test:cpp:cmd", "hello");
    auto r = client.Command("GET %s", "test:cpp:cmd");
    CHECK(r.IsOk() && r.Str == "hello", "Command generic");
    client.Del(std::vector<std::string>{"test:cpp:cmd"});

    // ==================== 结果汇总 ====================
    std::cout << "\n========================================" << std::endl;
    std::cout << "Passed: " << g_pass << std::endl;
    std::cout << "Failed: " << g_fail << std::endl;
    if (g_fail == 0) {
        std::cout << "All tests passed!" << std::endl;
        return 0;
    } else {
        std::cerr << "Some tests failed!" << std::endl;
        return 1;
    }
}
