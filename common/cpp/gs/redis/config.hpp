#pragma once

#include <string>
#include <vector>

namespace gs {
namespace redis {

// Config Redis 客户端配置
struct Config {
    std::vector<std::string> Addrs;   // Redis 地址列表，单节点填一个，集群填多个
    std::string              Password; // 密码，空表示无密码
    int                      PoolSize = 0; // 连接池大小，0 表示使用 hiredis 默认值
    int                      DB = 0;       // 数据库编号，仅对单节点有效
};

} // namespace redis
} // namespace gs
