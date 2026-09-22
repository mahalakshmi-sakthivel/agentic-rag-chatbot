#include "jwt_verifier.hpp"

namespace auth {

JwtVerifier::JwtVerifier(std::shared_ptr<TokenService> token_service, std::shared_ptr<SessionManager> session_manager)
    : token_service_(std::move(token_service)),
      session_manager_(std::move(session_manager))
{}

common::IdentityContext JwtVerifier::verify(const drogon::HttpRequestPtr& req) const {
    common::IdentityContext identity;
    identity.authenticated = false;

    // Extract Bearer token using Phase 1's helper
    std::string token = common::detail::extractBearerToken(req);
    if (token.empty()) {
        return identity;
    }

    // Phase 2 Validation: Verify JWT signature, expiration, algorithms, claims
    auto claims = token_service_->validate_token(token);
    if (!claims.has_value()) {
        return identity; // Token is invalid (expired, tampered, etc.)
    }

    // Phase 2 Validation: Fail-closed Tenant Validation
    if (claims->tenant_id.empty()) {
        return identity; // Reject request missing tenant ID
    }

    // Phase 2 Validation: Session <-> User Validation
    auto session = session_manager_->validate_session(claims->session_id);
    if (!session.has_value() || session->user_id != claims->user_id) {
        return identity; // Session is invalid, expired, revoked, or belongs to another user
    }

    // Populate common::IdentityContext on success
    identity.authenticated = true;
    identity.user_id = claims->user_id;
    identity.tenant_id = claims->tenant_id;
    identity.roles = claims->roles;
    identity.session_id = claims->session_id;

    return identity;
}

} // namespace auth
