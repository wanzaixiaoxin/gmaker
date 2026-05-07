#pragma once

#include <string>
#include <vector>

namespace gs {
namespace config {

// 简易 HTTP 客户端（Windows WinHTTP 封装）
// 仅支持同步 GET 请求，用于 ConfigWatcher 拉取配置
class HttpClient {
public:
    // 发送 GET 请求，返回响应体
    // 失败时返回空字符串，可通过 last_error 查看错误码
    static std::string Get(const std::string& url, long* last_error = nullptr);

private:
    static bool ParseUrl(const std::string& url, std::string& host, std::string& path, int& port, bool& https);
};

} // namespace config
} // namespace gs
