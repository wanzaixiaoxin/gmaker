#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <functional>

namespace gs {
namespace config {

class Loader {
public:
    explicit Loader(const std::string& path);

    bool Load();
    bool Reload();
    void SetOnReload(std::function<void()> fn);

    std::string GetString(const std::string& key, const std::string& def = "");
    int GetInt(const std::string& key, int def = 0);
    bool GetBool(const std::string& key, bool def = false);

private:
    std::mutex mtx_;
    std::string path_;
    std::unordered_map<std::string, std::string> data_;
    std::function<void()> on_reload_;
};

} // namespace config
} // namespace gs
