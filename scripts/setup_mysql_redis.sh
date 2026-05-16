#!/bin/bash
# WSL MySQL + Redis 一键安装脚本
# 使用方法: 在 WSL 终端中运行 bash /mnt/e/learn/github/gmaker/setup_mysql_redis.sh

set -e

echo "========================================="
echo "  WSL MySQL + Redis 安装脚本"
echo "========================================="
echo ""

# ====== 第 1 步：更换阿里云镜像源（加速下载）======
echo "===== [1/7] 更换阿里云镜像源 ====="
sudo cp /etc/apt/sources.list.d/ubuntu.sources /etc/apt/sources.list.d/ubuntu.sources.bak

sudo tee /etc/apt/sources.list.d/ubuntu.sources > /dev/null << 'SRCEOF'
Types: deb
URIs: https://mirrors.aliyun.com/ubuntu/
Suites: noble noble-updates noble-backports
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg

Types: deb
URIs: https://mirrors.aliyun.com/ubuntu/
Suites: noble-security
Components: main universe restricted multiverse
Signed-By: /usr/share/keyrings/ubuntu-archive-keyring.gpg
SRCEOF

echo "✓ 镜像源已更换为阿里云"

# ====== 第 2 步：更新软件源 ======
echo ""
echo "===== [2/7] 更新软件源 ====="
sudo apt-get update -y
echo "✓ 软件源更新完成"

# ====== 第 3 步：安装 MySQL ======
echo ""
echo "===== [3/7] 安装 MySQL Server ====="
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y mysql-server
echo "✓ MySQL 安装完成"

# ====== 第 4 步：配置 MySQL 允许外部访问 ======
echo ""
echo "===== [4/7] 配置 MySQL 允许外部访问 ====="
MYSQL_CNF="/etc/mysql/mysql.conf.d/mysqld.cnf"
if [ -f "$MYSQL_CNF" ]; then
    sudo sed -i 's/^bind-address.*/bind-address = 0.0.0.0/' "$MYSQL_CNF"
    sudo sed -i 's/^mysqlx-bind-address.*/mysqlx-bind-address = 0.0.0.0/' "$MYSQL_CNF"
    echo "✓ MySQL bind-address 已修改为 0.0.0.0"
fi

# ====== 第 5 步：启动 MySQL 并配置用户 ======
echo ""
echo "===== [5/7] 启动 MySQL 并创建远程用户 ====="
sudo service mysql start

sudo mysql -u root << 'SQLEOF'
ALTER USER 'root'@'localhost' IDENTIFIED WITH mysql_native_password BY 'root123456';
CREATE USER IF NOT EXISTS 'root'@'%' IDENTIFIED WITH mysql_native_password BY 'root123456';
GRANT ALL PRIVILEGES ON *.* TO 'root'@'%' WITH GRANT OPTION;
FLUSH PRIVILEGES;
SQLEOF

echo "✓ MySQL 用户配置完成 (root/root123456)"

# ====== 第 6 步：安装并配置 Redis ======
echo ""
echo "===== [6/7] 安装并配置 Redis ====="
sudo apt-get install -y redis-server

# 配置 Redis 允许外部访问
sudo sed -i 's/^bind 127.0.0.1/bind 0.0.0.0/' /etc/redis/redis.conf
sudo sed -i 's/^protected-mode yes/protected-mode no/' /etc/redis/redis.conf
echo "✓ Redis 安装配置完成"

# ====== 第 7 步：启动 Redis ======
echo ""
echo "===== [7/7] 启动 Redis ====="
sudo service redis-server start
echo "✓ Redis 已启动"

# ====== 验证 ======
echo ""
echo "========================================="
echo "  安装完成！服务状态："
echo "========================================="
echo ""
echo "MySQL 状态:"
sudo service mysql status || true
echo ""
echo "Redis 状态:"
sudo service redis-server status || true
echo ""

# 获取 WSL IP
WSL_IP=$(hostname -I | awk '{print $1}')
echo "========================================="
echo "  连接信息（从 Windows 连接使用以下信息）"
echo "========================================="
echo ""
echo "📦 MySQL:"
echo "   地址: localhost (或 $WSL_IP)"
echo "   端口: 3306"
echo "   用户: root"
echo "   密码: root123456"
echo ""
echo "📦 Redis:"
echo "   地址: localhost (或 $WSL_IP)"
echo "   端口: 6379"
echo "   密码: 无"
echo ""
echo "========================================="
echo "  快速测试命令"
echo "========================================="
echo "  测试 MySQL: mysql -h 127.0.0.1 -u root -proot123456 -e 'SELECT 1'"
echo "  测试 Redis: redis-cli ping"
echo "========================================="
