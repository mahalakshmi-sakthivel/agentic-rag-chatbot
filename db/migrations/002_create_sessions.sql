-- Migration: 002_create_sessions.sql
-- Sessions table for server-side session tracking.
--
-- Even with JWT (stateless tokens), sessions are tracked to support:
--   - Immediate logout / revocation
--   - Multiple session management
--   - Session expiration enforcement
--   - Security audit trail

CREATE TABLE IF NOT EXISTS sessions (
    id          TEXT PRIMARY KEY NOT NULL,               -- UUID v4 (session_id in JWT)
    user_id     TEXT NOT NULL                            -- Session owner
                REFERENCES users(id) ON DELETE CASCADE,
    created_at  TEXT NOT NULL DEFAULT (datetime('now')),
    expires_at  TEXT NOT NULL,                           -- ISO 8601 UTC
    status      TEXT NOT NULL DEFAULT 'active'           -- 'active' | 'revoked' | 'expired'
                CHECK(status IN ('active', 'revoked', 'expired'))
);

-- Indexes for fast lookup patterns
CREATE INDEX IF NOT EXISTS idx_sessions_user_id    ON sessions(user_id);
CREATE INDEX IF NOT EXISTS idx_sessions_status     ON sessions(status);
CREATE INDEX IF NOT EXISTS idx_sessions_expires_at ON sessions(expires_at);
