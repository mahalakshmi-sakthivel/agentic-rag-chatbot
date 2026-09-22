-- Migration: 001_create_users.sql
-- Users table for authentication.
--
-- Rules enforced:
--   - password_hash stores bcrypt hash only (NEVER plain text)
--   - email is unique
--   - status allows account disabling without deletion

CREATE TABLE IF NOT EXISTS users (
    id            TEXT PRIMARY KEY NOT NULL,          -- UUID v4
    email         TEXT UNIQUE NOT NULL,               -- Login identifier
    password_hash TEXT NOT NULL,                      -- bcrypt hash (NEVER plain text)
    tenant_id     TEXT NOT NULL DEFAULT '',           -- Tenant/application scope
    status        TEXT NOT NULL DEFAULT 'active'      -- 'active' | 'disabled'
                  CHECK(status IN ('active', 'disabled')),
    created_at    TEXT NOT NULL DEFAULT (datetime('now')),
    updated_at    TEXT NOT NULL DEFAULT (datetime('now'))
);

-- Index for fast email lookup during login
CREATE INDEX IF NOT EXISTS idx_users_email     ON users(email);
CREATE INDEX IF NOT EXISTS idx_users_tenant_id ON users(tenant_id);
CREATE INDEX IF NOT EXISTS idx_users_status    ON users(status);
