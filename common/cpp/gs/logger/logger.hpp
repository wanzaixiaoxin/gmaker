#pragma once

#include <string>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <mutex>

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

// Logger 轻量级结构化日志（MVP 版，输出 JSON 到 stdout）
class Logger {
public:
    Logger(std::string service, std::string node_id)
        : service_(std::move(service)), node_id_(std::move(node_id)) {}

    void SetLevel(Level l) { level_ = l; }

    template<typename... Args>
    void Log(Level level, const std::string& msg, Args... args) {
        if (level < level_) return;
        std::lock_guard<std::mutex> lk(mu_);
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        std::cout << "{"
                  << "\"time\":\"" << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S")
                  << "\",\"level\":\"" << LevelStr(level)
                  << "\",\"service\":\"" << service_
                  << "\",\"node_id\":\"" << node_id_
                  << "\",\"msg\":\"" << Escape(msg) << "\"}";
        AppendArgs(std::cout, args...);
        std::cout << std::endl;
    }

    void Debug(const std::string& msg) { Log(Level::Debug, msg); }
    void Info (const std::string& msg) { Log(Level::Info,  msg); }
    void Warn (const std::string& msg) { Log(Level::Warn,  msg); }
    void Error(const std::string& msg) { Log(Level::Error, msg); }
    void Fatal(const std::string& msg) { Log(Level::Fatal, msg); std::exit(1); }

private:
    static std::string Escape(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if (c == '"' || c == '\\') out += '\\';
            out += c;
        }
        return out;
    }

    static void AppendArgs(std::ostream&) {}

    template<typename K, typename V, typename... Rest>
    void AppendArgs(std::ostream& os, K k, V v, Rest... rest) {
        os << ",\"" << k << "\":\"" << v << "\"";
        AppendArgs(os, rest...);
    }

    std::string service_;
    std::string node_id_;
    Level level_ = Level::Info;
    std::mutex mu_;
};

} // namespace logger
} // namespace gs
