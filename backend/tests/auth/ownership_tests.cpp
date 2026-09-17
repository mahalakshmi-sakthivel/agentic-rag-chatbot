/**
 * @file ownership_tests.cpp
 * @brief Resource ownership and cross-user isolation tests (§42.4 of phase doc)
 *
 * Critical security tests:
 *   - User A → own resource → ALLOW
 *   - User A → User B resource → DENY
 *   - User B → User A resource → DENY
 *   - Direct object ID manipulation still denied
 *   - Cross-tenant access denied
 *   - Admin within same tenant → allowed
 *   - Admin across tenants → denied
 *   - Unauthenticated → denied
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "auth/authorization.hpp"
#include "auth/identity_context.hpp"
#include "auth/roles.hpp"

using namespace auth;

static IdentityContext make_user(const std::string& uid,
                                  const std::string& tenant = "tenant-1",
                                  const std::string& role   = "user") {
    IdentityContext id;
    id.user_id    = uid;
    id.tenant_id  = tenant;
    id.session_id = "s";
    id.roles      = {role};
    return id;
}

static ResourceContext make_resource(const std::string& rid,
                                      const std::string& owner,
                                      const std::string& tenant = "tenant-1") {
    return {rid, owner, tenant, "document"};
}

// ─────────────────────────────────────────────────────────────────────────────
// Own resource
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Ownership: user can access own resource", "[ownership]") {
    auto user     = make_user("user-A");
    auto resource = make_resource("doc-101", "user-A");
    REQUIRE(Authorization::can_access_resource(user, resource) == true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cross-user access — the critical security test
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Ownership: user cannot access another user's resource", "[ownership]") {
    auto user_A   = make_user("user-A");
    auto resource = make_resource("doc-202", "user-B");  // Owned by B
    REQUIRE(Authorization::can_access_resource(user_A, resource) == false);
}

TEST_CASE("Ownership: user-B cannot access user-A's resource", "[ownership]") {
    auto user_B   = make_user("user-B");
    auto resource = make_resource("doc-101", "user-A");
    REQUIRE(Authorization::can_access_resource(user_B, resource) == false);
}

// Direct object reference test (§37 of phase doc)
// A user changing the URL from /documents/doc-101 to /documents/doc-202
// must still be denied the latter.
TEST_CASE("Ownership: direct object reference manipulation is denied", "[ownership]") {
    auto user_A = make_user("user-A");
    // Original resource user_A can access
    auto own_resource   = make_resource("doc-101", "user-A");
    // Resource they do NOT own (URL manipulation attempt)
    auto other_resource = make_resource("doc-202", "user-B");

    REQUIRE(Authorization::can_access_resource(user_A, own_resource)   == true);
    REQUIRE(Authorization::can_access_resource(user_A, other_resource) == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Cross-tenant access
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Ownership: cross-tenant access is always denied", "[ownership]") {
    auto user_T1  = make_user("user-A", "tenant-1");
    auto resource = make_resource("doc-X", "user-A", "tenant-2");  // Same user, different tenant!
    REQUIRE(Authorization::can_access_resource(user_T1, resource) == false);
}

TEST_CASE("Ownership: admin cannot cross-tenant boundary", "[ownership]") {
    auto admin_T1 = make_user("admin-1", "tenant-1", "admin");
    auto resource = make_resource("doc-X", "user-A", "tenant-2");
    REQUIRE(Authorization::can_access_resource(admin_T1, resource) == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Admin within same tenant
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Ownership: admin can access resources in same tenant", "[ownership]") {
    auto admin    = make_user("admin-1", "tenant-1", "admin");
    auto resource = make_resource("doc-X", "user-B", "tenant-1");
    REQUIRE(Authorization::can_access_resource(admin, resource) == true);
}

// ─────────────────────────────────────────────────────────────────────────────
// Unauthenticated
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Ownership: unauthenticated identity cannot access any resource", "[ownership]") {
    auto unauth   = IdentityContext::unauthenticated();
    auto resource = make_resource("doc-101", "user-A");
    REQUIRE(Authorization::can_access_resource(unauth, resource) == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Require resource access (throws)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("Ownership: require_resource_access throws when denied", "[ownership]") {
    auto user_A   = make_user("user-A");
    auto resource = make_resource("doc-B", "user-B");
    REQUIRE_THROWS_AS(
        Authorization::require_resource_access(user_A, resource),
        std::runtime_error
    );
}

TEST_CASE("Ownership: require_resource_access does not throw when allowed", "[ownership]") {
    auto user_A   = make_user("user-A");
    auto resource = make_resource("doc-A", "user-A");
    REQUIRE_NOTHROW(Authorization::require_resource_access(user_A, resource));
}
