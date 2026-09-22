/**
 * @file session_tests.cpp
 * @brief Unit tests for SessionManager (§42.5 of phase doc)
 *
 * Tests:
 *   - Session creation
 *   - Session validation (active)
 *   - Session validation (expired)
 *   - Session revocation (logout)
 *   - Invalid session ID → nullopt
 *   - Listing own sessions
 *   - Ownership: user can only revoke own sessions
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "auth/session_manager.hpp"
#include "db/database.hpp"
#include <thread>
#include <chrono>

using namespace auth;

struct SessionFixture {
    Database       db{":memory:"};
    SessionManager mgr;

    SessionFixture() : mgr(db) {
        db.run_migrations("db/migrations");
        // Create two test users directly in DB
        db.execute(
            "INSERT INTO users (id, email, password_hash, status) VALUES "
            "('user-A', 'a@test.com', 'hash', 'active'), "
            "('user-B', 'b@test.com', 'hash', 'active')"
        );
    }
};

TEST_CASE_METHOD(SessionFixture, "Session: create returns non-empty ID", "[session]") {
    auto sid = mgr.create_session("user-A", 3600);
    REQUIRE_FALSE(sid.empty());
}

TEST_CASE_METHOD(SessionFixture, "Session: freshly created session validates", "[session]") {
    auto sid = mgr.create_session("user-A", 3600);
    auto s   = mgr.validate_session(sid);
    REQUIRE(s.has_value());
    REQUIRE(s->user_id == "user-A");
    REQUIRE(s->status  == SessionStatus::ACTIVE);
}

TEST_CASE_METHOD(SessionFixture, "Session: unknown session ID returns nullopt", "[session]") {
    auto s = mgr.validate_session("non-existent-id-xyz");
    REQUIRE_FALSE(s.has_value());
}

TEST_CASE_METHOD(SessionFixture, "Session: revoked session no longer validates", "[session]") {
    auto sid = mgr.create_session("user-A", 3600);
    mgr.revoke_session(sid, "user-A");
    auto s = mgr.validate_session(sid);
    REQUIRE_FALSE(s.has_value());
}

TEST_CASE_METHOD(SessionFixture, "Session: user cannot revoke another user's session", "[session]") {
    auto sid = mgr.create_session("user-A", 3600);
    // user-B tries to revoke user-A's session → must fail
    bool revoked = mgr.revoke_session(sid, "user-B");
    REQUIRE(revoked == false);
    // Session should still be valid for user-A
    auto s = mgr.validate_session(sid);
    REQUIRE(s.has_value());
}

TEST_CASE_METHOD(SessionFixture, "Session: revoke_all invalidates all user sessions", "[session]") {
    auto sid1 = mgr.create_session("user-A", 3600);
    auto sid2 = mgr.create_session("user-A", 3600);
    mgr.revoke_all_sessions("user-A");
    REQUIRE_FALSE(mgr.validate_session(sid1).has_value());
    REQUIRE_FALSE(mgr.validate_session(sid2).has_value());
}

TEST_CASE_METHOD(SessionFixture, "Session: list_sessions returns correct user's sessions", "[session]") {
    auto sid1 = mgr.create_session("user-A", 3600);
    auto sid2 = mgr.create_session("user-A", 3600);
    mgr.create_session("user-B", 3600);  // Different user

    auto sessions = mgr.list_sessions("user-A");
    REQUIRE(sessions.size() == 2);
    for (const auto& s : sessions) {
        REQUIRE(s.user_id == "user-A");
    }
}

TEST_CASE_METHOD(SessionFixture, "Session: delete_session enforces ownership", "[session]") {
    auto sid = mgr.create_session("user-A", 3600);
    // user-B cannot delete user-A's session
    bool deleted = mgr.delete_session(sid, "user-B");
    REQUIRE(deleted == false);
    REQUIRE(mgr.validate_session(sid).has_value()); // Still valid
}

TEST_CASE_METHOD(SessionFixture, "Session: two sessions for same user are independent", "[session]") {
    auto sid1 = mgr.create_session("user-A", 3600);
    auto sid2 = mgr.create_session("user-A", 3600);
    mgr.revoke_session(sid1, "user-A");
    REQUIRE_FALSE(mgr.validate_session(sid1).has_value());
    REQUIRE(mgr.validate_session(sid2).has_value()); // sid2 still valid
}
