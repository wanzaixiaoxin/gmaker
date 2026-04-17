#include "metrics.hpp"
#include <iostream>
#include <algorithm>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif
#include <thread>

namespace gs {
namespace metrics {

Counter* Registry::Counter(const std::string& name) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = counters_.find(name);
    if (it != counters_.end()) return it->second.get();
    auto c = std::make_unique<metrics::Counter>();
    auto* ptr = c.get();
    counters_[name] = std::move(c);
    return ptr;
}

Gauge* Registry::Gauge(const std::string& name) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = gauges_.find(name);
    if (it != gauges_.end()) return it->second.get();
    auto g = std::make_unique<metrics::Gauge>();
    auto* ptr = g.get();
    gauges_[name] = std::move(g);
    return ptr;
}

Histogram* Registry::Histogram(const std::string& name, const std::vector<int64_t>& buckets_ms) {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = histograms_.find(name);
    if (it != histograms_.end()) return it->second.get();
    auto h = std::make_unique<metrics::Histogram>(buckets_ms);
    auto* ptr = h.get();
    histograms_[name] = std::move(h);
    return ptr;
}

std::string Registry::PrometheusText() const {
    std::ostringstream out;
    std::lock_guard<std::mutex> lk(mtx_);

    for (const auto& [name, c] : counters_) {
        out << "# TYPE " << name << " counter\n";
        out << name << " " << c->Value() << "\n";
    }
    for (const auto& [name, g] : gauges_) {
        out << "# TYPE " << name << " gauge\n";
        out << name << " " << g->Value() << "\n";
    }
    for (const auto& [name, h] : histograms_) {
        auto s = h->GetSnapshot();
        out << "# TYPE " << name << " histogram\n";
        for (size_t i = 0; i < s.buckets.size(); ++i) {
            out << name << "_bucket{le=\"" << s.buckets[i] << "\"} " << s.counts[i] << "\n";
        }
        out << name << "_bucket{le=\"+Inf\"} " << s.count << "\n";
        out << name << "_sum " << s.sum << "\n";
        out << name << "_count " << s.count << "\n";
    }
    return out.str();
}

Registry* DefaultRegistry() {
    static Registry reg;
    return &reg;
}

static void HttpServeLoop(const std::string& addr, Registry* reg) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    size_t colon = addr.find(':');
    std::string host = (colon == 0) ? "0.0.0.0" : addr.substr(0, colon);
    int port = std::stoi(addr.substr(colon + 1));

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "[metrics] socket failed\n";
        return;
    }
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &sin.sin_addr);

    if (bind(fd, (sockaddr*)&sin, sizeof(sin)) != 0) {
        std::cerr << "[metrics] bind failed on " << addr << "\n";
#ifdef _WIN32
        closesocket(fd);
#else
        close(fd);
#endif
        return;
    }
    listen(fd, 5);
    std::cout << "[metrics] http server on " << addr << "/metrics\n";

    while (true) {
        sockaddr_in client{};
        socklen_t len = sizeof(client);
        int cfd = accept(fd, (sockaddr*)&client, &len);
        if (cfd < 0) continue;

        char buf[1024];
        int n = recv(cfd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            std::string text = reg->PrometheusText();
            std::ostringstream resp;
            resp << "HTTP/1.1 200 OK\r\n";
            resp << "Content-Type: text/plain; charset=utf-8\r\n";
            resp << "Content-Length: " << text.size() << "\r\n";
            resp << "Connection: close\r\n\r\n";
            resp << text;
            std::string s = resp.str();
            send(cfd, s.data(), (int)s.size(), 0);
        }
#ifdef _WIN32
        closesocket(cfd);
#else
        close(cfd);
#endif
    }
}

void ServeHTTP(const std::string& addr, Registry* reg) {
    std::thread([addr, reg]() { HttpServeLoop(addr, reg); }).detach();
}

} // namespace metrics
} // namespace gs
