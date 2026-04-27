package main

import (
	"context"
	"fmt"
	"time"

	"github.com/redis/go-redis/v9"
)

func main() {
	ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
	defer cancel()

	rdb := redis.NewClient(&redis.Options{Addr: "192.168.0.85:6379"})
	defer rdb.Close()

	_, err := rdb.Ping(ctx).Result()
	if err != nil {
		fmt.Println("Redis FAIL:", err)
	} else {
		fmt.Println("Redis OK: PONG")
	}
}
