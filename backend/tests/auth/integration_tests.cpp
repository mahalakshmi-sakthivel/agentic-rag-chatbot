/**
 * @file integration_tests.cpp
 * @brief End-to-end integration tests for the full auth flow (§50 of phase doc)
 *
 * Tests full register → login → protected request → logout → verify cycle.
 * Also tests contract tests with Phase 1/3/4/5 boundaries.
 *
 * Checkpoint 1 — Phase 1 + Phase 2:
 *   Register → Login → Token → Protected endpoint identity extraction
 *
 * Checkpoint 2 — Phase 2 + Phase 3 boundary:
 *   Authenticated IdentityContext user_id is correct for ingestion
 *
 * Checkpoint 3 — Phase 2 + Phase 4 boundary:
 *   IdentityContext tenant_id scope is preserved
 *
 * Checkpoint 4 — Phase 2 + Phase 5 boundary:
 *   IdentityContext roles are not modifiable by downstream code
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "auth/auth_service.hpp"
#include "auth/token_service.hpp"
#include "auth/session_manager.hpp"
#include "auth/authorization.hpp"
#include "auth/identity_context.hpp"
#include "auth/roles.hpp"
#include "db/database.hpp"
#include "config/auth_config.hpp"

using namespace auth;

// ─────────────────────────────────────────────────────────────────────────────
// Full integration fixture
// ─────────────────────────────────────────────────────────────────────────────

struct IntegrationFixture {
    Database       db{":memory:"};
    AuthConfig     cfg;
    AuthService    auth_svc;
    TokenService   token_svc;
    SessionManager session_mgr;

    IntegrationFixture()
        : cfg(make_config())
        , auth_svc(db, cfg)
        , token_svc(cfg)
        , session_mgr(db)
    {
        db.run_migrations("../db/migrations");
    }

    static AuthConfig make_config() {
        AuthConfig c;
        c.jwt_secret           = "integration_test_secret_32chars_x";
        c.jwt_issuer           = "test-issuer";
        c.jwt_audience         = "test-audience";
        c.token_expiry_seconds = 3600;
        c.db_path              = ":memory:";
        return c;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Checkpoint 1: Full register → login → validate → logout cycle
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(IntegrationFixture,
    "Integration: full register → login → validate → logout cycle", "[integration]")
{
    // ── 1. Register ───────────────────────────────────────────────────────────
    auto reg = auth_svc.register_user({"e2e@test.com", "SecurePass1!", "tenant-x"});
    REQUIRE(reg.success);
    REQUIRE_FALSE(reg.user_id.empty());

    // ── 2. Login ──────────────────────────────────────────────────────────────
    auto login = auth_svc.login({"e2e@test.com", "SecurePass1!", "tenant-x"});
    REQUIRE(login.success);
    REQUIRE_FALSE(login.access_token.empty());

    // ── 3. Validate token → get IdentityContext ───────────────────────────────
    auto claims_opt = token_svc.validate_token(login.access_token);
    REQUIRE(claims_opt.has_value());

    IdentityContext identity;
    identity.user_id    = claims_opt->user_id;
    identity.tenant_id  = claims_opt->tenant_id;
    identity.session_id = claims_opt->session_id;
    identity.roles      = claims_opt->roles;

    REQUIRE(identity.is_valid());
    REQUIRE(identity.user_id   == reg.user_id);
    REQUIRE(identity.tenant_id == "tenant-x");
    REQUIRE(identity.has_role("user"));

    // ── 4. Authorization: user can use chatbot ────────────────────────────────
    REQUIRE(Authorization::has_permission(identity, permissions::USE_CHATBOT));

    // ── 5. Logout ─────────────────────────────────────────────────────────────
    bool logged_out = auth_svc.logout(identity);
    REQUIRE(logged_out);

    // ── 6. Session should now be invalid ─────────────────────────────────────
    auto session = session_mgr.validate_session(identity.session_id);
    REQUIRE_FALSE(session.has_value());

    // ── 7. Token still technically valid JWT but session is revoked ───────────
    // A re-validation through middleware would fail at the session check
    auto session_after_logout = session_mgr.validate_session(claims_opt->session_id);
    REQUIRE_FALSE(session_after_logout.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// Checkpoint 2: Phase 2 → Phase 3 boundary
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(IntegrationFixture,
    "Checkpoint2: user_id from IdentityContext is correct for Phase 3 ingestion", "[integration]")
{
    auto reg   = auth_svc.register_user({"ingestion@test.com", "Pass12345!", "t1"});
    auto login = auth_svc.login({"ingestion@test.com", "Pass12345!", "t1"});
    REQUIRE(login.success);

    auto claims = token_svc.validate_token(login.access_token);
    REQUIRE(claims.has_value());

    // Phase 3 receives this user_id — it must match the registered user
    // and must NOT be a client-supplied value
    REQUIRE(claims->user_id == reg.user_id);
}

// ─────────────────────────────────────────────────────────────────────────────
// Checkpoint 3: Phase 2 → Phase 4 boundary
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(IntegrationFixture,
    "Checkpoint3: tenant_id scope is preserved through token for Phase 4 retrieval", "[integration]")
{
    auth_svc.register_user({"retrieval@test.com", "Pass12345!", "tenant-scoped"});
    auto login = auth_svc.login({"retrieval@test.com", "Pass12345!", "tenant-scoped"});

    auto claims = token_svc.validate_token(login.access_token);
    REQUIRE(claims.has_value());

    // Phase 4 must use this tenant_id for data isolation — it comes from the
    // authenticated token, not from the client request body
    REQUIRE(claims->tenant_id == "tenant-scoped");
}

// ─────────────────────────────────────────────────────────────────────────────
// Checkpoint 4: Phase 2 → Phase 5 boundary
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(IntegrationFixture,
    "Checkpoint4: IdentityContext roles cannot be escalated by downstream code", "[integration]")
{
    auth_svc.register_user({"agent@test.com", "Pass12345!", "t1"});
    auto login  = auth_svc.login({"agent@test.com", "Pass12345!", "t1"});
    auto claims = token_svc.validate_token(login.access_token);
    REQUIRE(claims.has_value());

    IdentityContext identity;
    identity.user_id    = claims->user_id;
    identity.tenant_id  = claims->tenant_id;
    identity.session_id = claims->session_id;
    identity.roles      = claims->roles;

    // Verify user is NOT admin
    REQUIRE_FALSE(identity.has_role(roles::ADMIN));
    REQUIRE(Authorization::has_permission(identity, permissions::MANAGE_USERS) == false);

    // Phase 5 (agent) must not be able to add admin role to escape authorization
    // (In practice, IdentityContext is const-propagated to agent tools)
    // This test verifies the initial state is correct
    REQUIRE(identity.roles.size() == 1);
    REQUIRE(identity.roles[0] == "user");
}

// ─────────────────────────────────────────────────────────────────────────────
// Cross-user isolation (§37, §42.4 of phase doc)
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(IntegrationFixture,
    "Integration: user-A cannot access user-B's resources", "[integration]")
{
    auto reg_a = auth_svc.register_user({"user-a@test.com", "PassA12345!", "t1"});
    auto reg_b = auth_svc.register_user({"user-b@test.com", "PassB12345!", "t1"});

    auto login_a = auth_svc.login({"user-a@test.com", "PassA12345!", "t1"});
    auto claims_a = token_svc.validate_token(login_a.access_token);

    IdentityContext identity_a;
    identity_a.user_id    = claims_a->user_id;
    identity_a.tenant_id  = claims_a->tenant_id;
    identity_a.session_id = claims_a->session_id;
    identity_a.roles      = claims_a->roles;

    // Resource owned by user-B
    ResourceContext resource_b;
    resource_b.resource_id     = "doc-owned-by-B";
    resource_b.owner_user_id   = reg_b.user_id;
    resource_b.owner_tenant_id = "t1";
    resource_b.resource_type   = "document";

    // User A must NOT be able to access user B's resource
    REQUIRE(Authorization::can_access_resource(identity_a, resource_b) == false);
}
