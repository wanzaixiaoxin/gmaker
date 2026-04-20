#include "config.hpp"
#include <toml.hpp>
#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>
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

// 使用 rapidjson 解析 JSON，输出扁平键值对（嵌套对象会被递归展平为 "a.b" 形式）
void FlattenJSONValue(const std::string& prefix, const rapidjson::Value& v,
                      std::unordered_map<std::string, std::string>& out) {
    if (v.IsObject()) {
        for (auto it = v.MemberBegin(); it != v.MemberEnd(); ++it) {
            std::string key = prefix.empty() ? it->name.GetString()
                                             : prefix + "." + it->name.GetString();
            FlattenJSONValue(key, it->value, out);
        }
    } else if (v.IsArray()) {
        // 数组直接序列化为 JSON 字符串片段（保持兼容）
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        v.Accept(writer);
        out[prefix] = buffer.GetString();
    } else if (v.IsString()) {
        out[prefix] = v.GetString();
    } else if (v.IsInt64()) {
        out[prefix] = std::to_string(v.GetInt64());
    } else if (v.IsUint64()) {
        out[prefix] = std::to_string(v.GetUint64());
    } else if (v.IsDouble()) {
        out[prefix] = std::to_string(v.GetDouble());
    } else if (v.IsBool()) {
        out[prefix] = v.GetBool() ? "true" : "false";
    } else if (v.IsNull()) {
        out[prefix] = "null";
    } else {
        // 回退：直接序列化
        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
        v.Accept(writer);
        out[prefix] = buffer.GetString();
    }
}

bool ParseFlatJSON(const std::string& content, std::unordered_map<std::string, std::string>& out) {
    rapidjson::Document doc;
    doc.Parse(content.c_str());
    if (doc.HasParseError()) {
        return false;
    }
    if (!doc.IsObject()) {
        return false;
    }
    FlattenJSONValue("", doc, out);
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
