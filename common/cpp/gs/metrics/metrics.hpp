#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <sstream>
#include <vector>

namespace gs {
namespace metrics {

// ==================== Counter ====================

class Counter {
public:
    void Inc() { val_.fetch_add(1, std::memory_order_relaxed); }
    void Add(int64_t n) { val_.fetch_add(n, std::memory_order_relaxed); }
    int64_t Value() const { return val_.load(std::memory_order_relaxed); }
private:
    std::atomic<int64_t> val_{0};
};

// ==================== Gauge ====================

class Gauge {
public:
    void Set(int64_t n) { val_.store(n, std::memory_order_relaxed); }
    void Inc() { val_.fetch_add(1, std::memory_order_relaxed); }
    void Dec() { val_.fetch_add(-1, std::memory_order_relaxed); }
    int64_t Value() const { return val_.load(std::memory_order_relaxed); }
private:
    std::atomic<int64_t> val_{0};
};

// ==================== Histogram ====================

class Histogram {
public:
    explicit Histogram(std::vector<int64_t> buckets_ms)
        : buckets_(std::move(buckets_ms)), counts_(buckets_.size(), 0) {}

    void Observe(double ms) {
        int64_t v = static_cast<int64_t>(ms);
        sum_.fetch_add(v, std::memory_order_relaxed);
        count_.fetch_add(1, std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(mtx_);
        for (size_t i = 0; i < buckets_.size(); ++i) {
            if (v <= buckets_[i]) {
                counts_[i]++;
            }
        }
    }

    struct Snapshot {
        int64_t sum = 0;
        int64_t count = 0;
        std::vector<int64_t> buckets;
        std::vector<int64_t> counts;
    };

    Snapshot GetSnapshot() const {
        Snapshot s;
        s.sum = sum_.load(std::memory_order_relaxed);
        s.count = count_.load(std::memory_order_relaxed);
        std::lock_guard<std::mutex> lk(mtx_);
        s.buckets = buckets_;
        s.counts = counts_;
        return s;
    }

private:
    std::vector<int64_t> buckets_;
    std::vector<int64_t> counts_;
    mutable std::mutex mtx_;
    std::atomic<int64_t> sum_{0};
    std::atomic<int64_t> count_{0};
};

// ==================== Registry ====================

class Registry {
public:
    Counter* Counter(const std::string& name);
    Gauge* Gauge(const std::string& name);
    Histogram* Histogram(const std::string& name, const std::vector<int64_t>& buckets_ms);

    std::string PrometheusText() const;

private:
    mutable std::mutex mtx_;
    std::map<std::string, std::unique_ptr<metrics::Counter>> counters_;
    std::map<std::string, std::unique_ptr<metrics::Gauge>> gauges_;
    std::map<std::string, std::unique_ptr<metrics::Histogram>> histograms_;
};

// 全局默认 Registry
Registry* DefaultRegistry();

// 启动 /metrics HTTP 服务（端口隔离），win32 用 threading + winsock
void ServeHTTP(const std::string& addr, Registry* reg);

inline Counter* DefaultCounter(const std::string& name) { return DefaultRegistry()->Counter(name); }
inline Gauge* DefaultGauge(const std::string& name) { return DefaultRegistry()->Gauge(name); }
inline Histogram* DefaultHistogram(const std::string& name, const std::vector<int64_t>& buckets) {
    return DefaultRegistry()->Histogram(name, buckets);
}
inline void ServeDefaultHTTP(const std::string& addr) { ServeHTTP(addr, DefaultRegistry()); }

} // namespace metrics
} // namespace gs
