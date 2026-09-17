#pragma once
/**
 * @file authentication_middleware.hpp
 * @brief Authentication middleware — validates tokens and creates IdentityContext.
 *
 * This middleware MUST run before any protected handler.
 *
 * Flow (§9 of phase doc):
 *   Incoming Request
 *     → Read "Authorization: Bearer <token>"
 *     → Validate token (signature, expiry, issuer, audience, algorithm)
 *     → Validate session (not revoked)
 *     → Extract identity
 *     → Create IdentityContext
 *     → Attach to request context
 *     → Continue OR return 401
 *
 * Fail closed: ANY validation failure → 401, request does NOT continue.
 */

#include "auth/token_service.hpp"
#include "auth/session_manager.hpp"
#include "auth/identity_context.hpp"
#include "config/auth_config.hpp"

#include <crow.h>
#include <optional>
#include <string>

namespace auth {
namespace middleware {

class AuthenticationMiddleware {
public:
    AuthenticationMiddleware(const TokenService& token_service,
                             const SessionManager& session_manager);

    /**
     * @brief Validate the request's Bearer token.
     *
     * Extracts IdentityContext and attaches it to the request via internal headers.
     * Returns 401 response on any failure.
     *
     * @param req     Incoming Crow request.
     * @param res     Crow response (set to 401 on failure).
     * @return true if authentication succeeded, false if it failed (and res was set).
     */
    bool authenticate(crow::request& req, crow::response& res) const;

private:
    const TokenService&    token_service_;
    const SessionManager&  session_manager_;

    static std::optional<std::string> extract_bearer_token(const crow::request& req);
    static void attach_identity(crow::request& req, const IdentityContext& identity);
    static crow::response unauthorized(const std::string& reason);
};

} // namespace middleware
} // namespace auth
