package redis

// Config Redis 客户端配置
type Config struct {
	Addrs    []string // Redis 地址列表，单节点填一个，集群填多个
	Password string   // 密码
	PoolSize int      // 连接池大小，0 表示使用默认值
}
