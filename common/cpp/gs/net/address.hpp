#pragma once

#include <string>
#include <vector>
#include <utility>
#include <cstdlib>

namespace gs {
namespace net {

// ParseAddrList 解析逗号分隔的 host:port 地址列表
// 格式: host1:port1,host2:port2
inline std::vector<std::pair<std::string, uint16_t>> ParseAddrList(const std::string& arg) {
    std::vector<std::pair<std::string, uint16_t>> out;
    size_t pos = 0;
    while (pos < arg.size()) {
        size_t comma = arg.find(',', pos);
        if (comma == std::string::npos) comma = arg.size();
        std::string pair = arg.substr(pos, comma - pos);
        size_t colon = pair.find(':');
        if (colon != std::string::npos) {
            out.push_back({pair.substr(0, colon),
                static_cast<uint16_t>(std::atoi(pair.substr(colon + 1).c_str()))});
        }
        pos = comma + 1;
    }
    return out;
}

} // namespace net
} // namespace gs
