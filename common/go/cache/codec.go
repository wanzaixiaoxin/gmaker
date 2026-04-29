package cache

import (
	"encoding/json"
	"fmt"
)

// Codec 定义对象与字符串之间的编解码接口
type Codec[T any] interface {
	Encode(v T) (string, error)
	Decode(s string) (T, error)
}

// JSONCodec 使用 JSON 的通用 Codec
type JSONCodec[T any] struct{}

func (c JSONCodec[T]) Encode(v T) (string, error) {
	b, err := json.Marshal(v)
	if err != nil {
		return "", err
	}
	return string(b), nil
}

func (c JSONCodec[T]) Decode(s string) (T, error) {
	var v T
	if err := json.Unmarshal([]byte(s), &v); err != nil {
		return v, err
	}
	return v, nil
}

// StringCodec 字符串透传 Codec
type StringCodec struct{}

func (c StringCodec) Encode(v string) (string, error) { return v, nil }
func (c StringCodec) Decode(s string) (string, error) { return s, nil }

// nilPlaceholder 用于缓存空值占位，防止缓存穿透
const nilPlaceholder = "__cache_nil__"

// IsNilPlaceholder 判断是否为空值占位
func IsNilPlaceholder(s string) bool {
	return s == nilPlaceholder
}

// ErrNotFound 表示底层存储未命中
type ErrNotFound struct {
	Key string
}

func (e ErrNotFound) Error() string {
	return fmt.Sprintf("cache key not found: %s", e.Key)
}
