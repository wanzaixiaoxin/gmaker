package discovery

import (
	"fmt"
	"strings"
)

// New 根据发现类型和地址创建 ServiceDiscovery 实例
// discoveryType: "registry" 或 "etcd"
// addrs: 地址列表，如 ["127.0.0.1:2379"]
func New(discoveryType string, addrs []string) (ServiceDiscovery, error) {
	switch strings.ToLower(discoveryType) {
	case "registry":
		return NewRegistryImpl(addrs), nil
	case "etcd":
		return NewEtcdImpl(addrs)
	default:
		return nil, fmt.Errorf("unsupported discovery type: %s (expected 'registry' or 'etcd')", discoveryType)
	}
}
