package config

import (
	"encoding/json"
	"fmt"
	"os"
)

// ServiceConfig 服务通用配置
type ServiceConfig struct {
	ServiceType string `json:"service_type"` // 服务类型
	NodeID      string `json:"node_id"`      // 节点 ID
	LogLevel    string `json:"log_level"`    // 日志级别
	LogFile     string `json:"log_file"`     // 日志文件
	MetricsAddr string `json:"metrics_addr"` // Metrics 地址
}

// NetworkConfig 网络配置
type NetworkConfig struct {
	Host           string `json:"host"`            // 监听地址
	Port           int    `json:"port"`            // 监听端口
	MaxConnections int    `json:"max_connections"` // 最大连接数
}

// RegistryConfig Registry 配置
type RegistryConfig struct {
	Nodes []string `json:"nodes"` // Registry 节点地址列表
}

// UpstreamNode 上游节点
type UpstreamNode struct {
	Host string `json:"host"`
	Port int    `json:"port"`
}

// UpstreamConfig 上游服务配置
type UpstreamConfig struct {
	Services map[string][]UpstreamNode `json:"services"` // 按服务类型分组
}

// GetServiceNodes 获取指定服务类型的节点列表
func (c *UpstreamConfig) GetServiceNodes(serviceType string) []UpstreamNode {
	if c.Services == nil {
		return nil
	}
	return c.Services[serviceType]
}

// CmdRangeConfig 命令范围配置
type CmdRangeConfig struct {
	Start uint32 `json:"start"`
	End   uint32 `json:"end"`
}

// SecurityConfig 安全配置
type SecurityConfig struct {
	MasterKeyHex        string `json:"master_key_hex"`         // 主密钥 (十六进制)
	ReplayWindowSeconds int    `json:"replay_window_seconds"` // 重放检查窗口
}

// LoadJSON 从 JSON 文件加载配置
func LoadJSON(path string, v interface{}) error {
	data, err := os.ReadFile(path)
	if err != nil {
		return fmt.Errorf("read config file: %w", err)
	}
	if err := json.Unmarshal(data, v); err != nil {
		return fmt.Errorf("parse config: %w", err)
	}
	return nil
}

// SaveJSON 保存配置到 JSON 文件
func SaveJSON(path string, v interface{}) error {
	data, err := json.MarshalIndent(v, "", "  ")
	if err != nil {
		return fmt.Errorf("marshal config: %w", err)
	}
	if err := os.WriteFile(path, data, 0644); err != nil {
		return fmt.Errorf("write config file: %w", err)
	}
	return nil
}
