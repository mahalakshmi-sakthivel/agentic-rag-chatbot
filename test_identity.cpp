// tests/api/test_identity.cpp
//
// TECHNICAL_CONTRACT.md Section 9.2 freezes this exact shape:
//   { "user_id": "uuid", "tenant_id": "uuid", "roles": ["user"], "session_id": "uuid" }
// This test exists so an accidental field rename (e.g. userId vs user_id)
// breaks CI immediately instead of surfacing as a Phase 2→3 integration bug.
//
#include "common/identity.h"
#include <gtest/gtest.h>

using common::IdentityContext;

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

TEST(IdentityContext, PassthroughAuthenticateReturnsUnauthenticatedContext)
{
    // Phase 1 stub: no request even needs to be well-formed since the body
    // never inspects it yet. nullptr is safe here because authenticate()
    // casts req to (void) — this documents that Phase 1 behavior, and will
    // correctly start failing once Phase 2 actually reads the request.
    const auto identity = common::authenticate(nullptr);
    EXPECT_FALSE(identity.authenticated);
}
