#include "config.hpp"
#include <toml.hpp>
#include <iostream>
#include <algorithm>
#include <fstream>
#include <sstream>

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

// 简易扁平 JSON 解析器（仅支持 string/number/bool/null 值，不支持嵌套对象/数组）
bool ParseFlatJSON(const std::string& content, std::unordered_map<std::string, std::string>& out) {
    size_t i = 0;
    auto skip_ws = [&]() {
        while (i < content.size() && (content[i] == ' ' || content[i] == '\n' || content[i] == '\r' || content[i] == '\t')) i++;
    };

    skip_ws();
    if (i >= content.size() || content[i] != '{') return false;
    i++;

    while (true) {
        skip_ws();
        if (i < content.size() && content[i] == '}') { i++; break; }

        if (i >= content.size() || content[i] != '"') return false;
        i++;
        std::string key;
        while (i < content.size() && content[i] != '"') {
            if (content[i] == '\\' && i + 1 < content.size()) {
                i++;
                switch (content[i]) {
                    case '"': case '\\': case '/': key += content[i]; break;
                    case 'b': key += '\b'; break;
                    case 'f': key += '\f'; break;
                    case 'n': key += '\n'; break;
                    case 'r': key += '\r'; break;
                    case 't': key += '\t'; break;
                    default: key += content[i]; break;
                }
            } else {
                key += content[i];
            }
            i++;
        }
        if (i >= content.size() || content[i] != '"') return false;
        i++;

        skip_ws();
        if (i >= content.size() || content[i] != ':') return false;
        i++;

        skip_ws();
        std::string value;
        if (i < content.size() && content[i] == '"') {
            i++;
            while (i < content.size() && content[i] != '"') {
                if (content[i] == '\\' && i + 1 < content.size()) {
                    i++;
                    switch (content[i]) {
                        case '"': case '\\': case '/': value += content[i]; break;
                        case 'b': value += '\b'; break;
                        case 'f': value += '\f'; break;
                        case 'n': value += '\n'; break;
                        case 'r': value += '\r'; break;
                        case 't': value += '\t'; break;
                        default: value += content[i]; break;
                    }
                } else {
                    value += content[i];
                }
                i++;
            }
            if (i >= content.size() || content[i] != '"') return false;
            i++;
        } else {
            while (i < content.size() && content[i] != ',' && content[i] != '}') {
                value += content[i];
                i++;
            }
            while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\n' || value.back() == '\r')) {
                value.pop_back();
            }
        }

        out[key] = value;

        skip_ws();
        if (i < content.size() && content[i] == ',') { i++; continue; }
        if (i < content.size() && content[i] == '}') { i++; break; }
        return false;
    }
    return true;
}

} // anonymous namespace

Loader::Loader(const std::string& path) : path_(path) {}

void Loader::SetOnReload(std::function<void()> fn) {
    on_reload_ = fn;
}

bool Loader::Load() {
    std::lock_guard<std::mutex> lk(mtx_);
    data_.clear();

    // 读取文件内容
    std::ifstream ifs(path_);
    if (!ifs) {
        std::cerr << "[config] failed to open: " << path_ << std::endl;
        return false;
    }
    std::stringstream buffer;
    buffer << ifs.rdbuf();
    std::string content = buffer.str();

    // 根据扩展名选择解析器
    bool is_json = false;
    if (path_.size() >= 5) {
        auto ext = path_.substr(path_.size() - 5);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        is_json = (ext == ".json");
    }

    if (is_json) {
        if (!ParseFlatJSON(content, data_)) {
            std::cerr << "[config] JSON parse failed: " << path_ << std::endl;
            return false;
        }
    } else {
        try {
            toml::value tbl = toml::parse(path_);
            for (const auto& [k, v] : tbl.as_table()) {
                std::string val = ValueToString(v);
                if (!val.empty()) {
                    data_[k] = val;
                }
            }
        } catch (const toml::syntax_error& e) {
            std::cerr << "[config] TOML parse failed: " << e.what() << std::endl;
            return false;
        } catch (const std::exception& e) {
            std::cerr << "[config] failed to load: " << e.what() << std::endl;
            return false;
        }
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
