// common/identity.h
//
// SEAM FOR PHASE 2 (Auth, Authorization & Session Management).
//
// Phase 1 threads an IdentityContext through every request but never
// enforces anything with it — `authenticate()` always returns an
// unauthenticated, pass-through context. Phase 2 replaces the body of
// authenticate() with real token/session validation and populates the
// remaining fields. No controller signature needs to change when that
// happens — they already call this and carry the result forward.
//
#pragma once

#include <drogon/HttpRequest.h>
#include <string>

namespace common
{

struct IdentityContext
{
    bool authenticated = false;
    std::string userId;
    std::string sessionId;
    // roles / permissions / tenant id land here in Phase 2.
};

inline IdentityContext authenticate(const drogon::HttpRequestPtr &req)
{
    (void)req;
    // Phase 1: no real auth yet — every request passes through.
    // Section 11: all endpoints except /v1/health are PLANNED to require
    // auth once Phase 2 lands; this function is where that check will live.
    return IdentityContext{};
}

} // namespace common
