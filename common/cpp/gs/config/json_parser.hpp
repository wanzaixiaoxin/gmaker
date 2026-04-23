#pragma once

#include "config.hpp"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/ostreamwrapper.h"
#include "rapidjson/writer.h"
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace gs {
namespace config {

// JSON 配置加载器 (使用 rapidjson)
class JsonLoader {
public:
    // 默认构造
    JsonLoader() : doc_() {}
    
    // 从文件加载
    static JsonLoader Load(const std::string& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("cannot open file: " + path);
        }
        
        rapidjson::IStreamWrapper isw(file);
        rapidjson::Document doc;
        doc.ParseStream(isw);
        
        if (doc.HasParseError()) {
            throw std::runtime_error("JSON parse error at offset " + 
                std::to_string(doc.GetErrorOffset()) + ": " + 
                std::to_string(doc.GetParseError()));
        }
        
        return JsonLoader(std::move(doc));
    }
    
    // 从字符串加载
    static JsonLoader FromString(const std::string& json) {
        rapidjson::Document doc;
        doc.Parse(json.c_str());
        
        if (doc.HasParseError()) {
            throw std::runtime_error("JSON parse error");
        }
        
        return JsonLoader(std::move(doc));
    }
    
    // 从 rapidjson::Document 移动构造
    JsonLoader(rapidjson::Document&& doc) : doc_(std::move(doc)) {}
    
    // 获取字符串
    std::string GetString(const char* key, const std::string& def = "") const {
        if (!doc_.HasMember(key)) return def;
        const auto& v = doc_[key];
        if (v.IsString()) return v.GetString();
        return def;
    }
    
    // 获取整数
    int64_t GetInt(const char* key, int64_t def = 0) const {
        if (!doc_.HasMember(key)) return def;
        const auto& v = doc_[key];
        if (v.IsInt64()) return v.GetInt64();
        if (v.IsInt()) return v.GetInt();
        if (v.IsUint64()) return static_cast<int64_t>(v.GetUint64());
        if (v.IsUint()) return v.GetUint();
        if (v.IsString()) {
            // 支持十六进制字符串
            std::string s = v.GetString();
            if (s.size() >= 2 && (s.substr(0, 2) == "0x" || s.substr(0, 2) == "0X")) {
                return static_cast<int64_t>(std::stoull(s, nullptr, 16));
            }
            return std::stoll(s);
        }
        return def;
    }
    
    // 获取布尔值
    bool GetBool(const char* key, bool def = false) const {
        if (!doc_.HasMember(key)) return def;
        const auto& v = doc_[key];
        if (v.IsBool()) return v.GetBool();
        return def;
    }
    
    // 获取子对象
    JsonLoader GetObject(const char* key) const {
        if (!doc_.HasMember(key)) {
            return JsonLoader();
        }
        const auto& v = doc_[key];
        if (v.IsObject()) {
            rapidjson::Document subDoc;
            subDoc.CopyFrom(v, subDoc.GetAllocator());
            return JsonLoader(std::move(subDoc));
        }
        return JsonLoader();
    }
    
    // 获取节点数组
    std::vector<UpstreamNode> GetNodeArray(const char* key) const {
        std::vector<UpstreamNode> nodes;
        if (!doc_.HasMember(key)) return nodes;
        
        const auto& arr = doc_[key];
        if (!arr.IsArray()) return nodes;
        
        for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
            const auto& item = arr[i];
            if (!item.IsObject()) continue;
            
            UpstreamNode node;
            if (item.HasMember("host") && item["host"].IsString()) {
                node.host = item["host"].GetString();
            }
            if (item.HasMember("port")) {
                const auto& port = item["port"];
                if (port.IsInt()) {
                    node.port = static_cast<uint16_t>(port.GetInt());
                } else if (port.IsUint()) {
                    node.port = static_cast<uint16_t>(port.GetUint());
                }
            }
            nodes.push_back(node);
        }
        return nodes;
    }
    
    // 获取字符串数组
    std::vector<std::string> GetStringArray(const char* key) const {
        std::vector<std::string> result;
        if (!doc_.HasMember(key)) return result;
        
        const auto& arr = doc_[key];
        if (!arr.IsArray()) return result;
        
        for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
            if (arr[i].IsString()) {
                result.push_back(arr[i].GetString());
            }
        }
        return result;
    }
    
    // 检查是否有成员
    bool Has(const char* key) const {
        return doc_.HasMember(key);
    }
    
    // 获取原始文档 (用于高级操作)
    const rapidjson::Value& GetDoc() const { return doc_; }
    
private:
    rapidjson::Document doc_;
};

// 加载 JSON 配置文件
inline JsonLoader LoadJson(const std::string& path) {
    return JsonLoader::Load(path);
}

} // namespace config
} // namespace gs
