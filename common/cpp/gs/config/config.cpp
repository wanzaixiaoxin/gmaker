#include "config.hpp"
#include <toml.hpp>
#include <iostream>
#include <algorithm>

namespace gs {
namespace config {

namespace {

std::string ValueToString(const toml::value& v) {
    if (v.is_string()) {
        return v.as_string();
    }
    if (v.is_integer()) {
        return std::to_string(v.as_integer());
    }
    if (v.is_floating()) {
        return std::to_string(v.as_floating());
    }
    if (v.is_boolean()) {
        return v.as_boolean() ? "true" : "false";
    }
    return "";
}

} // anonymous namespace

Loader::Loader(const std::string& path) : path_(path) {}

void Loader::SetOnReload(std::function<void()> fn) {
    on_reload_ = fn;
}

bool Loader::Load() {
    std::lock_guard<std::mutex> lk(mtx_);
    try {
        toml::value tbl = toml::parse(path_);
        data_.clear();
        for (const auto& [k, v] : tbl.as_table()) {
            std::string val = ValueToString(v);
            if (!val.empty()) {
                data_[k] = val;
            }
        }
    } catch (const toml::syntax_error& e) {
        std::cerr << "[config] parse failed: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "[config] failed to load: " << e.what() << std::endl;
        return false;
    }
    std::cout << "[config] loaded from " << path_ << std::endl;
    return true;
}

bool Loader::Reload() {
    if (!Load()) return false;
    if (on_reload_) {
        on_reload_();
    }
    return true;
}

std::string Loader::GetString(const std::string& key, const std::string& def) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = data_.find(key);
    return it != data_.end() ? it->second : def;
}

int Loader::GetInt(const std::string& key, int def) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = data_.find(key);
    if (it == data_.end()) return def;
    try {
        return std::stoi(it->second);
    } catch (...) {
        return def;
    }
}

bool Loader::GetBool(const std::string& key, bool def) {
    std::string v = GetString(key);
    std::transform(v.begin(), v.end(), v.begin(), ::tolower);
    if (v == "true" || v == "1" || v == "yes" || v == "on") return true;
    if (v == "false" || v == "0" || v == "no" || v == "off") return false;
    return def;
}

} // namespace config
} // namespace gs
