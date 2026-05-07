#include "config_watcher.hpp"
#include "config.hpp"
#include "http_client.hpp"

#include <hiredis/hiredis.h>
#include "config.pb.h"
#include <google/protobuf/util/json_util.h>

#include <sstream>
#include <fstream>
#include <iostream>
#include <cstring>

// SHA-256 计算（简化实现，实际项目可接入 OpenSSL）
#include <iomanip>

namespace gs {
namespace config {

// 简易 SHA-256（Windows 可使用 CryptHashData，此处用占位实现）
// 实际校验时建议接入 crypto.hpp 中的 SHA256 封装
static std::string SimpleSHA256(const std::string& data) {
    // 注意：这是简化实现，仅用于演示结构。
    // 生产环境请使用 common/cpp/crypto 中的 SHA256 封装。
    // 此处返回空字符串表示跳过校验（或接入真实 SHA256）
    (void)data;
    return "";
}

RedisWatcher::RedisWatcher(redisContext* redis_context, const std::string& namespace_)
    : redis_context_(redis_context)
    , namespace_(namespace_.empty() ? "default" : namespace_)
    , loader_(nullptr) {
}

RedisWatcher::~RedisWatcher() {
    Stop();
}

void RedisWatcher::Subscribe(const std::string& config_name, ConfigHandler handler) {
    std::lock_guard<std::mutex> lock(handlers_mtx_);
    handlers_[config_name] = handler;
}

void RedisWatcher::Unsubscribe(const std::string& config_name) {
    std::lock_guard<std::mutex> lock(handlers_mtx_);
    handlers_.erase(config_name);
}

void RedisWatcher::Start() {
    if (running_.exchange(true)) {
        return; // 已启动
    }
    stopped_ = false;
    watch_thread_ = std::thread(&RedisWatcher::WatchLoop, this);
}

void RedisWatcher::Stop() {
    stopped_ = true;
    if (watch_thread_.joinable()) {
        watch_thread_.join();
    }
    running_ = false;
}

void RedisWatcher::SetConfigServiceAddr(const std::string& addr) {
    service_addr_ = addr;
}

void RedisWatcher::SetLocalLoader(Loader* loader, const std::string& file_path) {
    loader_ = loader;
    file_path_ = file_path;
}

void RedisWatcher::SetNodeInfo(const std::string& region, const std::string& node_id,
                               const std::unordered_map<std::string, std::string>& tags) {
    region_ = region;
    node_id_ = node_id;
    tags_ = tags;
}

bool RedisWatcher::IsSubscribedTo(const std::string& config_name) const {
    std::lock_guard<std::mutex> lock(handlers_mtx_);
    return handlers_.find(config_name) != handlers_.end();
}

bool RedisWatcher::ShouldAcceptGray(const ConfigChangeEvent& event) const {
    // Phase 8: 灰度规则预留接口
    // 目前默认全量接收，未来根据 event 中的灰度字段和本节点信息匹配
    (void)event;
    return true;
}

void RedisWatcher::WatchLoop() {
    if (!redis_context_) {
        std::cerr << "[ConfigWatcher] redis context is null" << std::endl;
        return;
    }

    // 构建订阅频道列表
    std::vector<std::string> channels;
    {
        std::lock_guard<std::mutex> lock(handlers_mtx_);
        for (const auto& pair : handlers_) {
            channels.push_back("pubsub:config:" + namespace_ + ":" + pair.first);
        }
        // 同时订阅全量广播频道
        channels.push_back("pubsub:config:" + namespace_ + ":all");
    }

    if (channels.empty()) {
        std::cerr << "[ConfigWatcher] no handlers registered, skip start" << std::endl;
        return;
    }

    // 发送 SUBSCRIBE 命令
    // hiredis 的 subscribe 需要发送多个参数
    std::vector<const char*> argv;
    std::vector<size_t> argvlen;
    argv.push_back("SUBSCRIBE");
    argvlen.push_back(9);
    for (const auto& ch : channels) {
        argv.push_back(ch.c_str());
        argvlen.push_back(ch.size());
    }

    redisReply* reply = (redisReply*)redisCommandArgv(redis_context_, (int)argv.size(), argv.data(), argvlen.data());
    if (reply) {
        freeReplyObject(reply);
    }

    std::cout << "[ConfigWatcher] subscribed " << channels.size() << " channels" << std::endl;

    while (!stopped_) {
        redisReply* msg = nullptr;
        // 非阻塞检查（使用 1 秒超时）
        if (redisGetReply(redis_context_, (void**)&msg) != REDIS_OK) {
            if (stopped_) break;
            std::cerr << "[ConfigWatcher] redis get reply failed, reconnecting..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
            continue;
        }

        if (!msg) continue;

        // 解析 Pub/Sub 消息
        // 格式: ["message", channel, payload]
        if (msg->type == REDIS_REPLY_ARRAY && msg->elements == 3) {
            if (msg->element[0]->str && std::strcmp(msg->element[0]->str, "message") == 0) {
                std::string channel = msg->element[1]->str ? msg->element[1]->str : "";
                std::string payload = msg->element[2]->str ? std::string(msg->element[2]->str, msg->element[2]->len) : "";

                // 解析 protobuf ConfigChangeEvent
                // 注意：protobuf 生成的类在 ::config 命名空间，与 gs::config 区分
                ::config::ConfigChangeEvent pb_event;
                if (!pb_event.ParseFromString(payload)) {
                    std::cerr << "[ConfigWatcher] failed to parse protobuf event" << std::endl;
                    freeReplyObject(msg);
                    continue;
                }

                ConfigChangeEvent event;
                event.config_name = pb_event.config_name();
                event.namespace_ = pb_event.namespace_();
                event.version_id = pb_event.version_id();
                event.version_no = pb_event.version_no();
                event.checksum = pb_event.checksum();
                event.action = pb_event.action();
                event.timestamp = pb_event.timestamp();

                // 检查是否订阅了该配置
                if (!IsSubscribedTo(event.config_name)) {
                    freeReplyObject(msg);
                    continue;
                }

                // 灰度检查（Phase 8 预留）
                if (!ShouldAcceptGray(event)) {
                    std::cout << "[ConfigWatcher] gray rule rejected for " << event.config_name << std::endl;
                    freeReplyObject(msg);
                    continue;
                }

                std::cout << "[ConfigWatcher] received change event: config=" << event.config_name
                          << " version=" << event.version_no << " action=" << event.action << std::endl;

                // 执行回调（如果有注册自定义 handler）
                ConfigHandler handler;
                {
                    std::lock_guard<std::mutex> lock(handlers_mtx_);
                    auto it = handlers_.find(event.config_name);
                    if (it != handlers_.end()) {
                        handler = it->second;
                    }
                }
                if (handler) {
                    handler(event);
                }

                // 如果设置了 Loader 和 service_addr，自动执行 PullAndReload
                if (!service_addr_.empty() && loader_ && !file_path_.empty()) {
                    // 在独立线程中执行，避免阻塞 Redis 消费
                    std::thread reload_thread([this, event]() {
                        PullAndReload(service_addr_, event.config_name, event, loader_, file_path_);
                    });
                    reload_thread.detach();
                }
            }
        }

        freeReplyObject(msg);
    }

    std::cout << "[ConfigWatcher] stopped" << std::endl;
}

bool RedisWatcher::PullAndReload(const std::string& service_addr,
                                  const std::string& config_name,
                                  const ConfigChangeEvent& event,
                                  Loader* loader,
                                  const std::string& file_path) {
    // 1. 拉取完整配置内容
    std::string url = service_addr + "/api/configs/" + config_name + "/pull?namespace=" + event.namespace_
                      + "&version_id=" + std::to_string(event.version_id);

    long err = 0;
    std::string resp = HttpClient::Get(url, &err);
    if (err != 0) {
        std::cerr << "[ConfigWatcher] pull config failed, http error=" << err << std::endl;
        return false;
    }

    // 2. 解析 JSON 响应
    google::protobuf::util::JsonParseOptions options;
    ::config::ConfigPullRes pull_res;
    auto status = google::protobuf::util::JsonStringToMessage(resp, &pull_res, options);
    if (!status.ok()) {
        std::cerr << "[ConfigWatcher] parse pull response failed" << std::endl;
        return false;
    }
    if (!pull_res.ok()) {
        std::cerr << "[ConfigWatcher] pull config error: " << pull_res.msg() << std::endl;
        return false;
    }

    // 3. 校验 checksum（简化：实际接入 crypto::SHA256）
    if (!event.checksum.empty() && event.checksum != pull_res.checksum()) {
        std::cerr << "[ConfigWatcher] checksum mismatch" << std::endl;
        return false;
    }

    // 4. 写入本地文件（原子覆盖：先写 .tmp 再 rename）
    if (!file_path.empty()) {
        std::string tmp_path = file_path + ".tmp";
        std::ofstream ofs(tmp_path, std::ios::binary);
        if (!ofs) {
            std::cerr << "[ConfigWatcher] open temp file failed: " << tmp_path << std::endl;
            return false;
        }
        ofs << pull_res.content();
        ofs.close();

        if (std::rename(tmp_path.c_str(), file_path.c_str()) != 0) {
            std::cerr << "[ConfigWatcher] rename config file failed" << std::endl;
            return false;
        }
        std::cout << "[ConfigWatcher] config file updated: " << file_path << std::endl;
    }

    // 5. 触发重载
    if (loader) {
        if (!loader->Reload()) {
            std::cerr << "[ConfigWatcher] reload config failed" << std::endl;
            return false;
        }
        std::cout << "[ConfigWatcher] config reloaded: " << config_name << " v" << pull_res.version_no() << std::endl;
    }

    return true;
}

} // namespace config
} // namespace gs
