// common/identity.h
//
// SEAM FOR PHASE 2 (Auth, Authorization & Session Management).
//
// Shape is FROZEN by TECHNICAL_CONTRACT.md Section 9.2:
//   { "user_id": "uuid", "tenant_id": "uuid", "roles": ["user"], "session_id": "uuid" }
//
// ---------------------------------------------------------------------------
// WHAT CHANGED THIS ROUND (Team Lead review: "authenticate(req) is still a
// stub and protected routes don't enforce authentication")
// ---------------------------------------------------------------------------
// Phase 1 now REALLY enforces authentication on /v1/query and
// /v1/data/upload (see those controllers — they return 401 UNAUTHENTICATED
// when identity.authenticated is false). What Phase 1 still does NOT do,
// and per Section 9.1 must not do, is parse or verify a real JWT — "Only
// Phase 2 code parses/validates raw tokens."
//
// To make enforcement real without writing Phase 2's job, authenticate()
// now delegates to a swappable verifier function:
//
//   - Default verifier (devBypassVerifier, below): a documented, clearly
//     labeled, NON-SECURITY placeholder. No Authorization header -> rejected
//     (authenticated=false). A specific dev-only bypass header -> accepted,
//     but ONLY when AUTH_DEV_BYPASS=true in the environment (see
//     common/config.h) — this must be false in any shared/staging/
//     production environment (Section 19.1).
//   - Real verifier: once Phase 2's JWT verification function exists (as
//     its own module under src/auth/, picked up by the sibling-module CMake
//     discovery already in place), main.cpp calls
//     common::setTokenVerifier(auth::verifyJwtBearerToken) once at startup.
//     No controller changes needed — that's the entire point of the seam.
// ---------------------------------------------------------------------------
#pragma once

#include "config.h"
#include <drogon/HttpRequest.h>
#include <nlohmann/json.hpp>
#include <functional>
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

    // --- Phase-1/2-internal only, NOT part of the Section 9.2 wire shape
    //     (deliberately excluded from to_json below). True only when a
    //     verifier actually accepted the request. ---
    bool authenticated = false;
};

// Matches Section 9.2's JSON shape exactly. `authenticated` is intentionally
// NOT serialized — it's internal state, not a contract field.
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

using TokenVerifierFn = std::function<IdentityContext(const drogon::HttpRequestPtr &)>;

namespace detail
{
// Extracts the raw bearer token string from `Authorization: Bearer <token>`,
// or empty string if the header is missing/malformed. Shared by any
// verifier (including Phase 2's real one, if useful) so this parsing isn't
// duplicated.
inline std::string extractBearerToken(const drogon::HttpRequestPtr &req)
{
    if (!req) return {};
    const std::string header = std::string(req->getHeader("Authorization"));
    static const std::string prefix = "Bearer ";
    if (header.rfind(prefix, 0) != 0) return {};
    return header.substr(prefix.size());
}
} // namespace detail

// ---------------------------------------------------------------------------
// Phase 1's default verifier — NOT real security, and clearly not meant to
// be. Real behavior, real rejection, zero cryptography:
//
//   - No token, or token that isn't exactly "dev-<AUTH_DEV_BYPASS_TOKEN>"  ->
//     authenticated = false (caller returns 401).
//   - Token matches AND AppConfig::authDevBypassEnabled is true            ->
//     authenticated = true, with a synthetic identity, so Phase 1's own
//     tests and local integration work can exercise the "logged in" path
//     before Phase 2's real verifier exists.
//   - Token matches but AUTH_DEV_BYPASS is false (the required default
//     outside local dev, Section 19.1) -> still authenticated = false.
// ---------------------------------------------------------------------------
inline IdentityContext devBypassVerifier(const drogon::HttpRequestPtr &req)
{
    IdentityContext identity;

    if (!AppConfig::fromEnvironment().authDevBypassEnabled)
    {
        return identity; // unauthenticated, regardless of what was sent
    }

    const std::string token = detail::extractBearerToken(req);
    if (token != "dev-local-only")
    {
        return identity; // unauthenticated — wrong/missing token
    }

    identity.authenticated = true;
    identity.user_id = "dev-user";
    identity.tenant_id = "dev-tenant";
    identity.roles = {"user"};
    identity.session_id = "dev-session";
    return identity;
}

namespace detail
{
inline TokenVerifierFn &activeVerifier()
{
    static TokenVerifierFn verifier = devBypassVerifier;
    return verifier;
}
} // namespace detail

// PHASE 2 calls this once at startup (main.cpp) with its real JWT
// verification function once src/auth/ exists and is linked in. Until then
// it's never called, and devBypassVerifier remains active.
inline void setTokenVerifier(TokenVerifierFn verifier)
{
    detail::activeVerifier() = std::move(verifier);
}

inline IdentityContext authenticate(const drogon::HttpRequestPtr &req)
{
    return detail::activeVerifier()(req);
}

} // namespace common
