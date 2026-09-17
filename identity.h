// common/identity.h
//
// SEAM FOR PHASE 2 (Auth, Authorization & Session Management).
//
// Shape is FROZEN by TECHNICAL_CONTRACT.md Section 9.2 — every downstream
// module (Ingestion/Phase 3, Retrieval/Phase 4, Orchestrator/Phase 5)
// receives this struct, never a raw JWT:
//
//   { "user_id": "uuid", "tenant_id": "uuid", "roles": ["user"], "session_id": "uuid" }
//
// Phase 1 threads this through every request but never enforces anything
// with it — authenticate() always returns an empty, unauthenticated
// context. Phase 2 replaces the BODY of authenticate() with real JWT
// verification (Section 9.1) and populates all four fields. No caller
// (QueryController, UploadController, or anything downstream) needs its
// signature to change when that happens.
//
#pragma once

#include <drogon/HttpRequest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace common
{

struct IdentityContext
{
    // --- Section 9.2 contract fields — do not rename without a Section 27
    //     amendment, Phase 3/4/5 code will be written against these names. ---
    std::string user_id;
    std::string tenant_id;
    std::vector<std::string> roles;
    std::string session_id;

    // --- Phase-1-internal convenience field, NOT part of the Section 9.2
    //     wire shape (deliberately excluded from to_json below) — lets
    //     callers check "did real auth actually run" without inspecting
    //     individual fields. Always false until Phase 2 lands.
    bool authenticated = false;
};

// Matches Section 9.2's JSON shape exactly. `authenticated` is intentionally
// NOT serialized — it's Phase 1/2 internal state, not a contract field.
inline void to_json(nlohmann::json &j, const IdentityContext &identity)
{
    j = nlohmann::json{
        {"user_id", identity.user_id},
        {"tenant_id", identity.tenant_id},
        {"roles", identity.roles},
        {"session_id", identity.session_id}};
}

inline void from_json(const nlohmann::json &j, IdentityContext &identity)
{
    identity.user_id = j.value("user_id", std::string{});
    identity.tenant_id = j.value("tenant_id", std::string{});
    identity.roles = j.value("roles", std::vector<std::string>{});
    identity.session_id = j.value("session_id", std::string{});
}

// ---------------------------------------------------------------------------
// PHASE 2 INTEGRATION POINT
// ---------------------------------------------------------------------------
// Replace the body below with real verification of the
// `Authorization: Bearer <token>` header (Section 9.1). On success, populate
// user_id/tenant_id/roles/session_id from the verified JWT claims and set
// authenticated = true.
//
// On failure: this function should still return (not throw) — the CALLER
// (QueryController / UploadController) is responsible for checking
// `identity.authenticated` and returning `401 UNAUTHENTICATED`. Phase 1
// deliberately does not implement that check itself: per
// PHASE_1_CONTRACT.md Section 9 and parent Section 18.1, UNAUTHENTICATED
// and PERMISSION_DENIED are Phase 2's error codes to return, not Phase 1's.
// ---------------------------------------------------------------------------
inline IdentityContext authenticate(const drogon::HttpRequestPtr &req)
{
    (void)req;
    // Phase 1: no real auth yet — every request passes through unauthenticated.
    return IdentityContext{};
}

} // namespace common
