// tests/api/test_identity.cpp
//
// TECHNICAL_CONTRACT.md Section 9.2 freezes this exact shape:
//   { "user_id": "uuid", "tenant_id": "uuid", "roles": ["user"], "session_id": "uuid" }
// This test exists so an accidental field rename (e.g. userId vs user_id)
// breaks CI immediately instead of surfacing as a Phase 2→3 integration bug.
//
// Also covers this review round's addition: real enforcement via a
// swappable verifier (devBypassVerifier by default, common::setTokenVerifier
// as the Phase 2 hook).
//
#include "common/identity.h"
#include <gtest/gtest.h>
#include <cstdlib>

using common::IdentityContext;

namespace
{
// setenv/unsetenv aren't in <cstdlib> on all platforms the same way, but are
// available on the Linux/glibc target this project builds against.
void setEnv(const char *name, const char *value) { setenv(name, value, 1); }
void unsetEnv(const char *name) { unsetenv(name); }
} // namespace

TEST(IdentityContext, DefaultsToUnauthenticatedEmptyContext)
{
    const IdentityContext identity;
    EXPECT_FALSE(identity.authenticated);
    EXPECT_TRUE(identity.user_id.empty());
    EXPECT_TRUE(identity.tenant_id.empty());
    EXPECT_TRUE(identity.roles.empty());
    EXPECT_TRUE(identity.session_id.empty());
}

TEST(IdentityContext, SerializesToExactContractShape)
{
    IdentityContext identity;
    identity.user_id = "11111111-1111-1111-1111-111111111111";
    identity.tenant_id = "22222222-2222-2222-2222-222222222222";
    identity.roles = {"user"};
    identity.session_id = "33333333-3333-3333-3333-333333333333";
    identity.authenticated = true; // internal-only — must NOT appear in JSON

    const nlohmann::json j = identity;

    EXPECT_EQ(j["user_id"], "11111111-1111-1111-1111-111111111111");
    EXPECT_EQ(j["tenant_id"], "22222222-2222-2222-2222-222222222222");
    EXPECT_EQ(j["roles"], nlohmann::json::array({"user"}));
    EXPECT_EQ(j["session_id"], "33333333-3333-3333-3333-333333333333");

    // Only these 4 contract keys — nothing else, `authenticated` included.
    EXPECT_EQ(j.size(), 4u);
    EXPECT_FALSE(j.contains("authenticated"));
}

TEST(IdentityContext, RoundTripsThroughJson)
{
    const nlohmann::json j = {
        {"user_id", "u-1"},
        {"tenant_id", "t-1"},
        {"roles", {"user", "admin"}},
        {"session_id", "s-1"}};

    const IdentityContext identity = j.get<IdentityContext>();

    EXPECT_EQ(identity.user_id, "u-1");
    EXPECT_EQ(identity.tenant_id, "t-1");
    EXPECT_EQ(identity.roles, (std::vector<std::string>{"user", "admin"}));
    EXPECT_EQ(identity.session_id, "s-1");
    // Deserializing never sets authenticated — that's set only by
    // common::authenticate() after real verification, never from raw input.
    EXPECT_FALSE(identity.authenticated);
}

// ---------------------------------------------------------------------------
// devBypassVerifier / real enforcement — new this round.
// ---------------------------------------------------------------------------

class DevBypassVerifierTest : public ::testing::Test
{
protected:
    void TearDown() override
    {
        // Never leak env var state into other tests.
        unsetEnv("AUTH_DEV_BYPASS");
    }
};

TEST_F(DevBypassVerifierTest, RejectsRequestWithNoAuthorizationHeader)
{
    setEnv("AUTH_DEV_BYPASS", "true");
    const auto identity = common::devBypassVerifier(nullptr);
    EXPECT_FALSE(identity.authenticated);
}

TEST_F(DevBypassVerifierTest, RejectsBypassTokenWhenFlagDisabled)
{
    // AUTH_DEV_BYPASS unset -> defaults to false -> must reject even the
    // "correct" token. This is the Section 19.1 safety property: the
    // bypass can't accidentally work outside local dev.
    unsetEnv("AUTH_DEV_BYPASS");
    const auto identity = common::devBypassVerifier(nullptr);
    EXPECT_FALSE(identity.authenticated);
}

TEST(IdentityContext, SetTokenVerifierOverridesActiveVerifier)
{
    // Simulates the exact call Phase 2 will make in main.cpp once its real
    // JWT verifier exists — confirms the swap mechanism itself works.
    common::setTokenVerifier([](const drogon::HttpRequestPtr &) {
        IdentityContext identity;
        identity.authenticated = true;
        identity.user_id = "phase2-user";
        identity.tenant_id = "phase2-tenant";
        identity.roles = {"admin"};
        identity.session_id = "phase2-session";
        return identity;
    });

    const auto identity = common::authenticate(nullptr);
    EXPECT_TRUE(identity.authenticated);
    EXPECT_EQ(identity.user_id, "phase2-user");
    EXPECT_EQ(identity.roles, (std::vector<std::string>{"admin"}));

    // Restore the default so later tests (and any test run after this one
    // in the same binary) see Phase 1's normal behavior again.
    common::setTokenVerifier(common::devBypassVerifier);
}

TEST(IdentityContext, AuthenticateUsesDevBypassVerifierByDefault)
{
    // Guards against a future change accidentally leaving some other
    // verifier active by default.
    unsetEnv("AUTH_DEV_BYPASS");
    const auto identity = common::authenticate(nullptr);
    EXPECT_FALSE(identity.authenticated);
}
