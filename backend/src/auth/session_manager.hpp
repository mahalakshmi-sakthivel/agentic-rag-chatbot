#pragma once
/**
 * @file session_manager.hpp
 * @brief Server-side session tracking for logout/revocation support.
 *
 * Even with JWT (stateless tokens), we maintain a sessions table to support:
 *   - Immediate logout / revocation
 *   - Multiple session management (GET /sessions, DELETE /sessions/{id})
 *   - Session expiration enforcement
 *
 * Schema (see db/migrations/002_create_sessions.sql):
 *   sessions: id, user_id, created_at, expires_at, status
 */

#include "../common/identity.h"
#include "db/database.hpp"

#include <optional>
#include <string>
#include <vector>
#include <chrono>

namespace auth {

enum class SessionStatus {
    ACTIVE,
    EXPIRED,
    REVOKED
};

struct Session {
    std::string   id;
    std::string   user_id;
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point expires_at;
    SessionStatus status{SessionStatus::ACTIVE};
};

class SessionManager {
public:
    explicit SessionManager(Database& db);

    /**
     * @brief Create a new active session for a user.
     * @param user_id       Authenticated user's ID.
     * @param expiry_secs   Session lifetime in seconds.
     * @return The new session ID (UUID).
     * @throws std::runtime_error on database failure.
     */
    std::string create_session(const std::string& user_id, int expiry_secs);

    /**
     * @brief Check if a session is currently valid (active and not expired).
     * @param session_id  Session ID from the JWT "session_id" claim.
     * @return The session if valid, nullopt otherwise.
     */
    std::optional<Session> validate_session(const std::string& session_id) const noexcept;

    /**
     * @brief Invalidate (revoke) a session on logout.
     * @param session_id  Session to invalidate.
     * @param user_id     Must match the session's owner (ownership check).
     * @return true if the session was revoked, false if not found/not owned.
     */
    bool revoke_session(const std::string& session_id,
                        const std::string& user_id) noexcept;

    /**
     * @brief Revoke all active sessions for a user.
     */
    void revoke_all_sessions(const std::string& user_id) noexcept;

    /**
     * @brief List all sessions for a user.
     */
    std::vector<Session> list_sessions(const std::string& user_id) const;

    /**
     * @brief Delete a specific session by ID (ownership enforced).
     */
    bool delete_session(const std::string& session_id,
                        const std::string& requesting_user_id) noexcept;

private:
    Database& db_;

    std::string generate_session_id() const;
};

} // namespace auth
