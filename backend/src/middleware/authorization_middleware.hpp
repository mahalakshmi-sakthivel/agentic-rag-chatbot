#pragma once
/**
 * @file authorization_middleware.hpp
 * @brief Authorization middleware — enforces RBAC permissions per route.
 *
 * Runs AFTER AuthenticationMiddleware.
 * Never runs before authentication is confirmed.
 *
 * Usage: wrap protected route handlers with require_permission().
 *
 * Flow (§11 of phase doc):
 *   Authenticated Request
 *     → IdentityContext (from authentication middleware)
 *     → Required Permission check
 *     → Allowed? → Continue
 *     → Denied?  → 403
 */

#include "auth/identity_context.hpp"
#include "auth/authorization.hpp"

#include <crow.h>
#include <functional>
#include <string>

namespace auth {
namespace middleware {

class AuthorizationMiddleware {
public:
    /**
     * @brief Check if the request's identity has the required permission.
     *
     * @param req         Crow request (must have identity set by AuthenticationMiddleware).
     * @param res         Set to 403 if authorization fails.
     * @param permission  Required permission string (from permissions:: namespace).
     * @return true if authorized, false if forbidden (and res was set to 403).
     */
    static bool require_permission(const crow::request& req,
                                   crow::response& res,
                                   const std::string& permission);

    /**
     * @brief Check if the request's identity can access the given resource.
     */
    static bool require_resource_access(const crow::request& req,
                                        crow::response& res,
                                        const ResourceContext& resource);

private:
    static IdentityContext extract_identity_from_request(const crow::request& req);
    static crow::response forbidden();
};

} // namespace middleware
} // namespace auth
