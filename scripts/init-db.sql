CREATE DATABASE IF NOT EXISTS gmaker CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE gmaker;

CREATE TABLE IF NOT EXISTS players (
    player_id BIGINT PRIMARY KEY,
    account VARCHAR(64) UNIQUE NOT NULL,
    password VARCHAR(128) NOT NULL,
    nickname VARCHAR(64) NOT NULL,
    level INT DEFAULT 1,
    exp BIGINT DEFAULT 0,
    coin BIGINT DEFAULT 0,
    diamond BIGINT DEFAULT 0,
    create_at BIGINT DEFAULT 0,
    login_at BIGINT DEFAULT 0
);

CREATE TABLE IF NOT EXISTS chat_rooms (
    room_id BIGINT PRIMARY KEY,
    name VARCHAR(128) NOT NULL,
    creator_id BIGINT NOT NULL,
    status TINYINT DEFAULT 0,
    created_at BIGINT DEFAULT 0,
    closed_at BIGINT DEFAULT 0
);
