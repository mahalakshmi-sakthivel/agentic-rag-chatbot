/**
 * @file authorization_tests.cpp
 * @brief Unit tests for RBAC authorization (§42.3 of phase doc)
 *
 * Tests:
 *   - User with permission → allowed
 *   - User without permission → denied
 *   - Admin-only operation by normal user → denied
 *   - Admin has admin permissions
 *   - Missing/empty roles → denied (fail closed)
 *   - Invalid role → denied
 *   - Unauthenticated identity → denied
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "auth/authorization.hpp"
#include "auth/identity_context.hpp"
#include "auth/roles.hpp"

using namespace auth;

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build an IdentityContext
// ─────────────────────────────────────────────────────────────────────────────

static IdentityContext make_identity(const std::string& user_id,
                                     const std::vector<std::string>& roles,
                                     const std::string& tenant = "tenant-1") {
    IdentityContext id;
    id.user_id    = user_id;
    id.tenant_id  = tenant;
    id.session_id = "session-x";
    id.roles      = roles;
    return id;
}

// ─────────────────────────────────────────────────────────────────────────────
// has_permission — User role
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Authorization: user can use chatbot", "[authz]") {
    auto id = make_identity("u1", {roles::USER});
    REQUIRE(Authorization::has_permission(id, permissions::USE_CHATBOT) == true);
}

TEST_CASE("Authorization: user can view own sessions", "[authz]") {
    auto id = make_identity("u1", {roles::USER});
    REQUIRE(Authorization::has_permission(id, permissions::VIEW_OWN_SESSIONS) == true);
}

TEST_CASE("Authorization: user cannot manage other users (admin-only)", "[authz]") {
    auto id = make_identity("u1", {roles::USER});
    REQUIRE(Authorization::has_permission(id, permissions::MANAGE_USERS) == false);
}

TEST_CASE("Authorization: user cannot access admin config", "[authz]") {
    auto id = make_identity("u1", {roles::USER});
    REQUIRE(Authorization::has_permission(id, permissions::ADMIN_CONFIG) == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// has_permission — Admin role
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Authorization: admin can manage users", "[authz]") {
    auto id = make_identity("admin1", {roles::ADMIN});
    REQUIRE(Authorization::has_permission(id, permissions::MANAGE_USERS) == true);
}

TEST_CASE("Authorization: admin can also use chatbot", "[authz]") {
    auto id = make_identity("admin1", {roles::ADMIN});
    REQUIRE(Authorization::has_permission(id, permissions::USE_CHATBOT) == true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Fail closed
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Authorization: unauthenticated identity is denied all permissions", "[authz]") {
    auto id = IdentityContext::unauthenticated();
    REQUIRE(Authorization::has_permission(id, permissions::USE_CHATBOT)  == false);
    REQUIRE(Authorization::has_permission(id, permissions::LOGIN)        == false);
    REQUIRE(Authorization::has_permission(id, permissions::MANAGE_USERS) == false);
}

TEST_CASE("Authorization: unknown role gets no permissions (fail closed)", "[authz]") {
    auto id = make_identity("u2", {"super_hacker_role"});
    REQUIRE(Authorization::has_permission(id, permissions::USE_CHATBOT)  == false);
    REQUIRE(Authorization::has_permission(id, permissions::MANAGE_USERS) == false);
}

TEST_CASE("Authorization: empty roles gets no permissions", "[authz]") {
    auto id = make_identity("u3", {});
    REQUIRE(Authorization::has_permission(id, permissions::USE_CHATBOT) == false);
}

TEST_CASE("Authorization: empty permission string returns false", "[authz]") {
    auto id = make_identity("u1", {roles::ADMIN});
    REQUIRE(Authorization::has_permission(id, "") == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// require_permission (throws on denial)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Authorization: require_permission throws when denied", "[authz]") {
    auto id = make_identity("u1", {roles::USER});
    REQUIRE_THROWS_AS(
        Authorization::require_permission(id, permissions::MANAGE_USERS),
        std::runtime_error
    );
}

TEST_CASE("Authorization: require_permission does not throw when allowed", "[authz]") {
    auto id = make_identity("u1", {roles::USER});
    REQUIRE_NOTHROW(Authorization::require_permission(id, permissions::USE_CHATBOT));
}
