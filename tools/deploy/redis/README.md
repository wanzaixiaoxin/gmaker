# Redis Cluster 部署

## 快速启动

```bash
cd tools/deploy/redis
docker-compose up -d
bash init-cluster.sh
```

## 验证

```bash
docker exec -it redis-node-1 redis-cli -p 6371 cluster info
docker exec -it redis-node-1 redis-cli -p 6371 cluster nodes
```

## 停止/清理

```bash
docker-compose down
# 清理数据（谨慎操作）
rm -rf data/node-*/
```

## 连接地址

本地开发可直接连接任意节点：
- `127.0.0.1:6371` ~ `127.0.0.1:6376`
