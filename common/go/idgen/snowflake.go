package idgen

import (
	"errors"
	"sync"
	"time"
)

// Snowflake 标准雪花算法实现
type Snowflake struct {
	mu        sync.Mutex
	epoch     int64 // 起始时间戳（毫秒）
	nodeID    int64
	sequence  int64
	lastTime  int64
}

const (
	nodeBits     = 10
	sequenceBits = 12
	nodeMax      = int64(-1) ^ (int64(-1) << nodeBits)
	sequenceMask = int64(-1) ^ (int64(-1) << sequenceBits)
	timeShift    = nodeBits + sequenceBits
	nodeShift    = sequenceBits
)

// Epoch 2024-01-01 00:00:00 UTC
var defaultEpoch = time.Date(2024, 1, 1, 0, 0, 0, 0, time.UTC).UnixMilli()

// NewSnowflake 创建 Snowflake 生成器
func NewSnowflake(nodeID int64) (*Snowflake, error) {
	if nodeID < 0 || nodeID > nodeMax {
		return nil, errors.New("node ID out of range")
	}
	return &Snowflake{
		epoch:    defaultEpoch,
		nodeID:   nodeID,
		sequence: 0,
		lastTime: -1,
	}, nil
}

// NextID 生成下一个唯一 ID
func (s *Snowflake) NextID() (int64, error) {
	s.mu.Lock()
	defer s.mu.Unlock()

	now := time.Now().UnixMilli()
	if now < s.lastTime {
		return 0, errors.New("clock moved backwards")
	}

	if now == s.lastTime {
		s.sequence = (s.sequence + 1) & sequenceMask
		if s.sequence == 0 {
			// 序列号溢出，等待下一毫秒
			for now <= s.lastTime {
				now = time.Now().UnixMilli()
			}
		}
	} else {
		s.sequence = 0
	}

	s.lastTime = now
	id := (now-s.epoch)<<timeShift | (s.nodeID << nodeShift) | s.sequence
	return id, nil
}
