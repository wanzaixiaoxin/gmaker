#pragma once

#include <string>
#include <vector>
#include <functional>
#include <thread>
#include <atomic>
#include <unordered_map>
#include <mutex>

// forward declare hiredis
struct redisContext;
struct redisReply;

namespace gs {
namespace config {

class Loader;

// 配置变更事件（对应 protobuf ConfigChangeEvent）
struct ConfigChangeEvent {
    std::string config_name;
    std::string namespace_;
    int64_t version_id = 0;
    int32_t version_no = 0;
    std::string checksum;
    std::string action;     // "publish" or "rollback"
    int64_t timestamp = 0;
};

// 配置变更回调类型
typedef std::function<void(const ConfigChangeEvent& event)> ConfigHandler;

// Redis 配置监听器
// 基于 hiredis 的 Pub/Sub 订阅，后台线程阻塞监听 Redis 消息
class RedisWatcher {
public:
    // redis_context: hiredis redisContext* 指针
    // namespace_: 配置命名空间，默认 "default"
    RedisWatcher(redisContext* redis_context, const std::string& namespace_ = "default");
    ~RedisWatcher();

    // 禁止拷贝
    RedisWatcher(const RedisWatcher&) = delete;
    RedisWatcher& operator=(const RedisWatcher&) = delete;

    // 注册对指定配置名称的变更监听
    void Subscribe(const std::string& config_name, ConfigHandler handler);

    // 取消对指定配置的监听
    void Unsubscribe(const std::string& config_name);

    // 启动后台监听线程
    void Start();

    // 停止监听（线程安全）
    void Stop();

    // 设置 Config Service HTTP 地址，用于 PullAndReload
    void SetConfigServiceAddr(const std::string& addr);

    // 设置本地配置文件路径和 Loader（用于 Reload）
    void SetLocalLoader(Loader* loader, const std::string& file_path);

    // 设置节点身份信息（用于灰度匹配）
    void SetNodeInfo(const std::string& region, const std::string& node_id,
                     const std::unordered_map<std::string, std::string>& tags);

    // 手动拉取并重载配置（同步阻塞）
    static bool PullAndReload(const std::string& service_addr,
                              const std::string& config_name,
                              const ConfigChangeEvent& event,
                              Loader* loader,
                              const std::string& file_path);

private:
    void WatchLoop();
    bool IsSubscribedTo(const std::string& config_name) const;
    bool ShouldAcceptGray(const ConfigChangeEvent& event) const;

    redisContext* redis_context_;
    std::string namespace_;
    std::string service_addr_;
    Loader* loader_;
    std::string file_path_;

    std::string region_;
    std::string node_id_;
    std::unordered_map<std::string, std::string> tags_;

    std::unordered_map<std::string, ConfigHandler> handlers_;
    mutable std::mutex handlers_mtx_;

    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
    std::thread watch_thread_;
};

} // namespace config
} // namespace gs
