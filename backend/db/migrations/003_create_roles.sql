-- Migration: 003_create_roles.sql
-- RBAC roles and user-role assignments.
--
-- Roles: 'user' (default), 'admin'
-- A user may have multiple roles (many-to-many via user_roles).

CREATE TABLE IF NOT EXISTS roles (
    name        TEXT PRIMARY KEY NOT NULL,   -- 'user' | 'admin'
    description TEXT NOT NULL DEFAULT ''
);

-- Seed built-in roles
INSERT OR IGNORE INTO roles (name, description) VALUES
    ('user',  'Standard chatbot user'),
    ('admin', 'Administrator with user-management privileges');

-- User-role assignments
CREATE TABLE IF NOT EXISTS user_roles (
    user_id  TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    role     TEXT NOT NULL REFERENCES roles(name) ON DELETE CASCADE,
    PRIMARY KEY (user_id, role)
);

CREATE INDEX IF NOT EXISTS idx_user_roles_user_id ON user_roles(user_id);
CREATE INDEX IF NOT EXISTS idx_user_roles_role    ON user_roles(role);
