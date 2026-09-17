/**
 * @file authorization_middleware.cpp
 */

#include "authorization_middleware.hpp"

#include <sstream>
#include <iostream>

namespace auth {
namespace middleware {

bool AuthorizationMiddleware::require_permission(const crow::request& req,
                                                  crow::response& res,
                                                  const std::string& permission) {
    auto identity = extract_identity_from_request(req);
    if (!identity.is_valid()) {
        // Should not reach here if AuthenticationMiddleware ran, but fail closed anyway
        res = forbidden();
        return false;
    }

    if (!Authorization::has_permission(identity, permission)) {
        std::cout << "[authz_middleware] event=permission_denied"
                  << " user_id=" << identity.user_id
                  << " permission=" << permission << "\n";
        res = forbidden();
        return false;
    }

    return true;
}

bool AuthorizationMiddleware::require_resource_access(const crow::request& req,
                                                       crow::response& res,
                                                       const ResourceContext& resource) {
    auto identity = extract_identity_from_request(req);
    if (!identity.is_valid()) {
        res = forbidden();
        return false;
    }

    if (!Authorization::can_access_resource(identity, resource)) {
        std::cout << "[authz_middleware] event=resource_access_denied"
                  << " user_id=" << identity.user_id
                  << " resource_id=" << resource.resource_id << "\n";
        res = forbidden();
        return false;
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

IdentityContext AuthorizationMiddleware::extract_identity_from_request(
    const crow::request& req)
{
    std::string uid = req.get_header_value("X-Identity-User-Id");
    if (uid.empty()) return IdentityContext::unauthenticated();

    IdentityContext identity;
    identity.user_id    = uid;
    identity.tenant_id  = req.get_header_value("X-Identity-Tenant-Id");
    identity.session_id = req.get_header_value("X-Identity-Session-Id");

    std::string roles_str = req.get_header_value("X-Identity-Roles");
    std::istringstream ss(roles_str);
    std::string role;
    while (std::getline(ss, role, ',')) {
        if (!role.empty()) identity.roles.push_back(role);
    }
    return identity;
}

crow::response AuthorizationMiddleware::forbidden() {
    crow::json::wvalue body;
    body["error"]  = "Forbidden";
    body["status"] = 403;
    auto resp = crow::response(403, body.dump());
    resp.set_header("Content-Type", "application/json");
    return resp;
}

} // namespace middleware
} // namespace auth
