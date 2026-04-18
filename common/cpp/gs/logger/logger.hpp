#pragma once

#include <string>
#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>
#include <vector>
#include <ctime>

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
typealias Field = std::pair<std::string, std::string>;

// Logger 轻量级结构化日志（输出 JSON 到 stdout）
class Logger {
public:
    Logger(std::string service, std::string node_id)
        : service_(std::move(service)), node_id_(std::move(node_id)) {}

    void SetLevel(Level l) { level_ = l; }

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
        std::lock_guard<std::mutex> lk(mu_);
        EmitLog(level, msg, args...);
    }

    // 基础日志方法
    void Debug(const std::string& msg) { Log(Level::Debug, msg); }
    void Info (const std::string& msg) { Log(Level::Info,  msg); }
    void Warn (const std::string& msg) { Log(Level::Warn,  msg); }
    void Error(const std::string& msg) { Log(Level::Error, msg); }
    void Fatal(const std::string& msg) { Log(Level::Fatal, msg); std::exit(1); }

    // 格式化日志方法
    template<typename... Args>
    void Debugf(const std::string& fmt, Args...&& args) {
        if (Level::Debug < level_) return;
        std::lock_guard<std::mutex> lk(mu_);
        EmitLog(Level::Debug, Format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void Infof(const std::string& fmt, Args...&& args) {
        if (Level::Info < level_) return;
        std::lock_guard<std::mutex> lk(mu_);
        EmitLog(Level::Info, Format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void Warnf(const std::string& fmt, Args...&& args) {
        if (Level::Warn < level_) return;
        std::lock_guard<std::mutex> lk(mu_);
        EmitLog(Level::Warn, Format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void Errorf(const std::string& fmt, Args...&& args) {
        if (Level::Error < level_) return;
        std::lock_guard<std::mutex> lk(mu_);
        EmitLog(Level::Error, Format(fmt, std::forward<Args>(args)...));
    }

    template<typename... Args>
    void Fatalf(const std::string& fmt, Args...&& args) {
        if (Level::Fatal < level_) return;
        std::lock_guard<std::mutex> lk(mu_);
        EmitLog(Level::Fatal, Format(fmt, std::forward<Args>(args)...));
        std::exit(1);
    }

private:
    // 统一输出逻辑
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

        std::cout << "{"
                  << "\"time\":\"" << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
                  << "." << std::setfill('0') << std::setw(3) << ms.count() << "Z"
                  << "\",\"level\":\"" << LevelStr(level)
                  << "\",\"service\":\"" << Escape(service_)
                  << "\",\"node_id\":\"" << Escape(node_id_)
                  << "\",\"msg\":\"" << Escape(msg) << "\"";

        // 子 Logger 的固定字段
        for (const auto& f : fields_) {
            std::cout << ",\"" << f.first << "\":\"" << Escape(f.second) << "\"";
        }

        AppendArgs(std::cout, args...);
        std::cout << "}" << std::endl;
    }

    // 完整的 JSON 字符串转义（处理控制字符、引号、反斜杠）
    static std::string Escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (unsigned char c : s) {
            switch (c) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (c < 0x20) {
                        // 其他控制字符用 \u00xx 表示
                        char buf[7];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        out += buf;
                    } else {
                        out += static_cast<char>(c);
                    }
            }
        }
        return out;
    }

    static void AppendArgs(std::ostream&) {}

    template<typename K, typename V, typename... Rest>
    void AppendArgs(std::ostream& os, K k, V v, Rest... rest) {
        os << ",\"" << k << "\":\"" << Escape(std::to_string(v)) << "\"";
        AppendArgs(os, rest...);
    }

    // std::string 特化，避免 to_string 二次转义
    template<typename K, typename... Rest>
    void AppendArgs(std::ostream& os, K k, const std::string& v, Rest... rest) {
        os << ",\"" << k << "\":\"" << Escape(v) << "\"";
        AppendArgs(os, rest...);
    }

    // 格式化字符串（简化版 printf）
    template<typename... Args>
    static std::string Format(const std::string& fmt, Args...&& args) {
        // 使用 snprintf 计算所需大小
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
    mutable std::mutex mu_;
};

} // namespace logger
} // namespace gs
