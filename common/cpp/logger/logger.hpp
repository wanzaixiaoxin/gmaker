#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <vector>
#include <ctime>
#include <fstream>
#include <memory>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace gs {
namespace logger {

enum class Level { Debug, Info, Warn, Error, Fatal };

inline const char* LevelStr(Level l) {
    switch (l) {
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO";
        case Level::Warn:  return "WARN";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "UNKNOWN";
}

// 结构化日志字段
using Field = std::pair<std::string, std::string>;

// Logger 轻量级结构化日志（支持输出到 stdout 或文件）
class Logger {
public:
    Logger(std::string service, std::string node_id)
        : service_(std::move(service)), node_id_(std::move(node_id)), mu_(std::make_shared<std::mutex>()) {}

    void SetLevel(Level l) { level_ = l; }

    // 设置日志输出文件（空字符串则输出到 stdout）
    void SetOutputFile(const std::string& path) {
        std::lock_guard<std::mutex> lk(*mu_);
        if (path.empty()) {
            file_out_.reset();
            return;
        }
        auto fs = std::make_shared<std::ofstream>(path, std::ios::app);
        if (fs->is_open()) {
            file_out_ = std::move(fs);
        }
    }

    // 创建带有固定字段的子 Logger（值拷贝，线程安全）
    Logger With(const std::string& key, const std::string& value) const {
        Logger child = *this;
        child.fields_.push_back({key, value});
        return child;
    }

    // 快捷方式：附加 trace_id 字段
    Logger WithTrace(const std::string& traceID) const {
        return With("trace_id", traceID);
    }

    template<typename... Args>
    void Log(Level level, const std::string& msg, Args... args) {
        if (level < level_) return;
        std::lock_guard<std::mutex> lk(*mu_);
        EmitLog(level, msg, args...);
    }

    // 基础日志方法
    void Debug(const std::string& msg) { Log(Level::Debug, msg); }
    void Info (const std::string& msg) { Log(Level::Info,  msg); }
    void Warn (const std::string& msg) { Log(Level::Warn,  msg); }
    void Error(const std::string& msg) { Log(Level::Error, msg); }
    void Fatal(const std::string& msg) { Log(Level::Fatal, msg); std::exit(1); }

    // 通用格式化日志方法
    template<typename... Args>
    void Logf(Level level, const std::string& fmt, Args&&... args) {
        if (level < level_) return;
        std::lock_guard<std::mutex> lk(*mu_);
        EmitLog(level, Format(fmt, std::forward<Args>(args)...));
    }

    // 格式化日志快捷方法
    template<typename... Args> void Debugf(const std::string& fmt, Args&&... args) { Logf(Level::Debug, fmt, std::forward<Args>(args)...); }
    template<typename... Args> void Infof (const std::string& fmt, Args&&... args) { Logf(Level::Info,  fmt, std::forward<Args>(args)...); }
    template<typename... Args> void Warnf (const std::string& fmt, Args&&... args) { Logf(Level::Warn,  fmt, std::forward<Args>(args)...); }
    template<typename... Args> void Errorf(const std::string& fmt, Args&&... args) { Logf(Level::Error, fmt, std::forward<Args>(args)...); }
    template<typename... Args> void Fatalf(const std::string& fmt, Args&&... args) { Logf(Level::Fatal, fmt, std::forward<Args>(args)...); std::exit(1); }

private:
    // rapidjson Writer 的数值写入辅助函数
    template<typename T>
    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, const T& v) {
        writer.String(std::to_string(v).c_str());
    }

    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, const std::string& v) {
        writer.String(v.c_str());
    }

    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, const char* v) {
        writer.String(v);
    }

    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, bool v) {
        writer.Bool(v);
    }

    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, int v) { writer.Int(v); }
    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, long v) { writer.Int64(v); }
    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, long long v) { writer.Int64(v); }
    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, unsigned int v) { writer.Uint(v); }
    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, unsigned long v) { writer.Uint64(v); }
    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, unsigned long long v) { writer.Uint64(v); }
    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, double v) { writer.Double(v); }
    static void WriteJSONValue(rapidjson::Writer<rapidjson::StringBuffer>& writer, float v) { writer.Double(v); }

    static void AppendArgs(rapidjson::Writer<rapidjson::StringBuffer>&) {}

    template<typename K, typename V, typename... Rest>
    void AppendArgs(rapidjson::Writer<rapidjson::StringBuffer>& writer, K k, V v, Rest... rest) {
        writer.Key(k);
        WriteJSONValue(writer, v);
        AppendArgs(writer, rest...);
    }

    // 统一输出逻辑（使用 rapidjson 生成标准 JSON）
    template<typename... Args>
    void EmitLog(Level level, const std::string& msg, Args... args) {
        auto now = std::chrono::system_clock::now();
        auto sec = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;

        std::tm tm{};
#ifdef _WIN32
        gmtime_s(&tm, &sec);
#else
        gmtime_r(&sec, &tm);
#endif

        char time_buf[32];
        std::strftime(time_buf, sizeof(time_buf), "%Y-%m-%dT%H:%M:%S", &tm);
        std::string time_str = std::string(time_buf) + "." +
            (ms.count() < 100 ? "0" : "") +
            (ms.count() < 10 ? "0" : "") +
            std::to_string(ms.count()) + "Z";

        rapidjson::StringBuffer buffer;
        rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);

        writer.StartObject();
        writer.Key("time");
        writer.String(time_str.c_str());
        writer.Key("level");
        writer.String(LevelStr(level));
        writer.Key("service");
        writer.String(service_.c_str());
        writer.Key("node_id");
        writer.String(node_id_.c_str());
        writer.Key("msg");
        writer.String(msg.c_str());

        // 子 Logger 的固定字段
        for (const auto& f : fields_) {
            writer.Key(f.first.c_str());
            writer.String(f.second.c_str());
        }

        AppendArgs(writer, args...);
        writer.EndObject();

        if (file_out_ && file_out_->is_open()) {
            *file_out_ << buffer.GetString() << std::endl;
        } else {
            std::cout << buffer.GetString() << std::endl;
        }
    }

    // 格式化字符串（简化版 printf）
    template<typename... Args>
    static std::string Format(const std::string& fmt, Args&&... args) {
        int size = std::snprintf(nullptr, 0, fmt.c_str(), std::forward<Args>(args)...);
        if (size < 0) return fmt;
        std::string buf(size, '\0');
        std::snprintf(&buf[0], buf.size() + 1, fmt.c_str(), std::forward<Args>(args)...);
        return buf;
    }

    std::string service_;
    std::string node_id_;
    Level level_ = Level::Info;
    std::vector<Field> fields_;
    std::shared_ptr<std::mutex> mu_;
    std::shared_ptr<std::ofstream> file_out_;
};

// ParseLogLevel 从字符串解析日志级别（不区分大小写）
inline Level ParseLogLevel(const std::string& s) {
    if (s == "debug") return Level::Debug;
    if (s == "warn")  return Level::Warn;
    if (s == "error") return Level::Error;
    if (s == "fatal") return Level::Fatal;
    return Level::Info;
}

} // namespace logger
} // namespace gs
