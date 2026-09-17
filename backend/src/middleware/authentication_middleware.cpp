/**
 * @file authentication_middleware.cpp
 */

#include "authentication_middleware.hpp"

#include <iostream>
#include <sstream>

namespace auth {
namespace middleware {

AuthenticationMiddleware::AuthenticationMiddleware(
    const TokenService& token_service,
    const SessionManager& session_manager)
    : token_service_(token_service)
    , session_manager_(session_manager)
{}

bool AuthenticationMiddleware::authenticate(crow::request& req,
                                             crow::response& res) const {
    // ── Step 1: Extract Bearer token ─────────────────────────────────────────
    auto token_opt = extract_bearer_token(req);
    if (!token_opt.has_value()) {
        res = unauthorized("Authentication required");
        return false;
    }

    // ── Step 2: Validate token (signature, expiry, issuer, audience, algo) ───
    // NOTE: raw token is NOT logged
    auto claims_opt = token_service_.validate_token(*token_opt);
    if (!claims_opt.has_value()) {
        res = unauthorized("Invalid or expired token");
        return false;
    }

    // ── Step 3: Validate session (not revoked) ────────────────────────────────
    auto session_opt = session_manager_.validate_session(claims_opt->session_id);
    if (!session_opt.has_value()) {
        res = unauthorized("Session expired or revoked");
        return false;
    }

    // ── Step 4: Build IdentityContext ─────────────────────────────────────────
    IdentityContext identity;
    identity.user_id    = claims_opt->user_id;
    identity.tenant_id  = claims_opt->tenant_id;
    identity.session_id = claims_opt->session_id;
    identity.roles      = claims_opt->roles;

    // ── Step 5: Attach identity to request ────────────────────────────────────
    attach_identity(req, identity);

    // Log authentication event (NOT the token)
    std::cout << "[auth_middleware] event=authenticate status=success"
              << " user_id=" << identity.user_id
              << " session_id=" << identity.session_id << "\n";

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::optional<std::string> AuthenticationMiddleware::extract_bearer_token(
    const crow::request& req)
{
    std::string auth_header = req.get_header_value("Authorization");
    if (auth_header.empty()) return std::nullopt;

    // Expect "Bearer <token>"
    const std::string prefix = "Bearer ";
    if (auth_header.size() <= prefix.size() ||
        auth_header.substr(0, prefix.size()) != prefix)
    {
        return std::nullopt;
    }

    std::string token = auth_header.substr(prefix.size());
    if (token.empty()) return std::nullopt;

    return token;
}

void AuthenticationMiddleware::attach_identity(crow::request& req,
                                               const IdentityContext& identity) {
    // Attach validated identity as internal request headers so controllers can read it.
    // These headers are never set by the client — they're overwritten here.
    req.add_header("X-Identity-User-Id",    identity.user_id);
    req.add_header("X-Identity-Tenant-Id",  identity.tenant_id);
    req.add_header("X-Identity-Session-Id", identity.session_id);

    // Roles as comma-separated string
    std::ostringstream roles_ss;
    for (size_t i = 0; i < identity.roles.size(); ++i) {
        if (i > 0) roles_ss << ",";
        roles_ss << identity.roles[i];
    }
    req.add_header("X-Identity-Roles", roles_ss.str());
}

crow::response AuthenticationMiddleware::unauthorized(const std::string& reason) {
    crow::json::wvalue body;
    body["error"]  = reason;
    body["status"] = 401;
    auto resp = crow::response(401, body.dump());
    resp.set_header("Content-Type", "application/json");
    resp.set_header("WWW-Authenticate", "Bearer");
    return resp;
}

} // namespace middleware
} // namespace auth
