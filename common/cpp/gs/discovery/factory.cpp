#include "factory.hpp"
#include "registry_impl.hpp"

#ifdef ENABLE_ETCD_DISCOVERY
#include "etcd_impl.hpp"
#endif

#include <iostream>

namespace gs {
namespace discovery {

std::unique_ptr<ServiceDiscovery> CreateDiscovery(
    const std::string& type,
    const std::vector<std::string>& addrs) {

    if (type == "registry") {
        std::vector<std::pair<std::string, uint16_t>> reg_addrs;
        for (const auto& addr : addrs) {
            auto pos = addr.find(':');
            if (pos != std::string::npos) {
                reg_addrs.emplace_back(
                    addr.substr(0, pos),
                    static_cast<uint16_t>(std::stoi(addr.substr(pos + 1))));
            }
        }
        return std::make_unique<RegistryImpl>(nullptr, reg_addrs);
    }

#ifdef ENABLE_ETCD_DISCOVERY
    if (type == "etcd") {
        return std::make_unique<EtcdImpl>(addrs);
    }
#endif

    std::cerr << "CreateDiscovery: unsupported type '" << type << "'" << std::endl;
    return nullptr;
}

} // namespace discovery
} // namespace gs
