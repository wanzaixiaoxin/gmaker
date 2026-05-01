CREATE DATABASE IF NOT EXISTS gmaker CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
USE gmaker;

-- ========================================
-- 配置管理中心表（config-go 服务专用）
-- ========================================

CREATE TABLE IF NOT EXISTS configs (
    id              BIGINT AUTO_INCREMENT PRIMARY KEY,
    name            VARCHAR(128) NOT NULL COMMENT '配置唯一标识（如 item_table）',
    namespace       VARCHAR(64)  NOT NULL DEFAULT 'default' COMMENT '命名空间',
    format          VARCHAR(16)  NOT NULL DEFAULT 'json' COMMENT '数据格式：json/toml/yaml',
    schema_def      JSON         NULL COMMENT 'JSON Schema 校验规则（可选）',
    description     VARCHAR(256) NULL,
    current_version BIGINT       NOT NULL DEFAULT 0 COMMENT '当前生效的版本号（关联 config_versions.id）',
    status          TINYINT      NOT NULL DEFAULT 0 COMMENT '0=正常, 1=禁用',
    created_at      BIGINT       NOT NULL DEFAULT 0,
    updated_at      BIGINT       NOT NULL DEFAULT 0,
    UNIQUE KEY uk_name_ns (name, namespace)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS config_versions (
    id           BIGINT AUTO_INCREMENT PRIMARY KEY,
    config_id    BIGINT       NOT NULL COMMENT '关联 configs.id',
    version      INT          NOT NULL DEFAULT 1 COMMENT '单调递增版本号',
    content      LONGTEXT     NOT NULL COMMENT '配置内容原文',
    checksum     VARCHAR(64)  NOT NULL COMMENT 'SHA-256(content)',
    status       TINYINT      NOT NULL DEFAULT 0 COMMENT '0=草稿, 1=已发布, 2=已回滚, 3=已废弃',
    published_at BIGINT       NULL COMMENT '发布时间',
    created_by   VARCHAR(64)  NOT NULL COMMENT '操作人',
    created_at   BIGINT       NOT NULL DEFAULT 0,
    UNIQUE KEY uk_config_ver (config_id, version)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS config_logs (
    id          BIGINT AUTO_INCREMENT PRIMARY KEY,
    config_id   BIGINT       NOT NULL,
    version_id  BIGINT       NULL COMMENT '关联 config_versions.id',
    action      VARCHAR(32)  NOT NULL COMMENT 'create|edit|save_draft|publish|rollback|delete',
    operator    VARCHAR(64)  NOT NULL COMMENT '操作人账号',
    detail      JSON         NULL COMMENT '变更详情：diff摘要、目标服务范围等',
    ip          VARCHAR(64)  NULL,
    created_at  BIGINT       NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

CREATE TABLE IF NOT EXISTS config_subscribers (
    id             BIGINT AUTO_INCREMENT PRIMARY KEY,
    config_name    VARCHAR(128) NOT NULL,
    service_type   VARCHAR(64)  NOT NULL COMMENT 'biz/gateway/realtime/login 等',
    subscribe_mode VARCHAR(32)  NOT NULL DEFAULT 'exact' COMMENT 'exact|prefix|glob',
    created_at     BIGINT       NOT NULL DEFAULT 0,
    UNIQUE KEY uk_sub (config_name, service_type)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci;

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
