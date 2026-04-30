CREATE DATABASE IF NOT EXISTS gmaker CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE gmaker;

-- 账号认证表（login-go 专用）
CREATE TABLE IF NOT EXISTS accounts (
    player_id BIGINT PRIMARY KEY,
    account VARCHAR(64) UNIQUE NOT NULL,
    password VARCHAR(128) NOT NULL,
    status TINYINT DEFAULT 0 COMMENT '0=正常,1=冻结,2=注销',
    create_at BIGINT DEFAULT 0
);

-- 用户业务资料表（biz-go 专用）
CREATE TABLE IF NOT EXISTS player_profiles (
    player_id BIGINT PRIMARY KEY,
    nickname VARCHAR(64) NOT NULL,
    level INT DEFAULT 1,
    exp BIGINT DEFAULT 0,
    coin BIGINT DEFAULT 0,
    diamond BIGINT DEFAULT 0,
    is_bot TINYINT DEFAULT 0 COMMENT '0=普通用户, 1=机器人',
    create_at BIGINT DEFAULT 0,
    login_at BIGINT DEFAULT 0
);

-- 机器人账号管理表（bot-go 专用）
CREATE TABLE IF NOT EXISTS bot_accounts (
    bot_id INT AUTO_INCREMENT PRIMARY KEY,
    player_id BIGINT NOT NULL UNIQUE,
    bot_type VARCHAR(32) NOT NULL DEFAULT 'chatbot' COMMENT '机器人类型：chatbot / npc / moderator',
    config JSON,
    status TINYINT DEFAULT 0 COMMENT '0=启用, 1=禁用',
    create_at BIGINT DEFAULT 0
);

CREATE TABLE IF NOT EXISTS chat_rooms (
    room_id BIGINT PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    creator_id BIGINT NOT NULL,
    status TINYINT DEFAULT 0,
    created_at BIGINT DEFAULT 0,
    closed_at BIGINT DEFAULT 0
);
