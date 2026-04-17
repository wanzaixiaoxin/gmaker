#!/bin/bash
# 初始化 Redis Cluster（3 主 3 从）
# 用法：docker-compose up -d 之后执行此脚本

echo "Waiting for Redis nodes to start..."
sleep 3

docker exec -it redis-node-1 redis-cli --cluster create \
  redis-node-1:6371 redis-node-2:6372 redis-node-3:6373 \
  redis-node-4:6374 redis-node-5:6375 redis-node-6:6376 \
  --cluster-replicas 1 --cluster-yes

echo "Redis cluster init done."
echo "Check with: docker exec -it redis-node-1 redis-cli -p 6371 cluster info"
