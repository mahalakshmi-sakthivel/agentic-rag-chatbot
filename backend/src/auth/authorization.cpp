/**
 * @file authorization.cpp
 */

#include "authorization.hpp"

#include <stdexcept>
#include <algorithm>

namespace auth {

bool Authorization::has_permission(const common::IdentityContext& identity,
                                   const std::string& permission) noexcept {
    // ── Fail closed — unauthenticated identity has no permissions ─────────────
    if (!identity.authenticated) return false;
    if (permission.empty())   return false;

    // ── Derive permissions from roles ─────────────────────────────────────────
    // Permissions are determined server-side from roles — never from the client
    auto perms = permissions_for_roles(identity.roles);
    return perms.count(permission) > 0;
}

bool Authorization::can_access_resource(const common::IdentityContext& identity,
                                         const ResourceContext& resource) noexcept {
    // ── Fail closed ───────────────────────────────────────────────────────────
    if (!identity.authenticated) return false;

    // ── Tenant scope check (cross-tenant access is always denied) ─────────────
    // If both have a tenant_id, they must match
    if (!identity.tenant_id.empty() && !resource.owner_tenant_id.empty()) {
        if (identity.tenant_id != resource.owner_tenant_id) {
            return false; // Cross-tenant: DENY
        }
    }

    // ── Own resource ──────────────────────────────────────────────────────────
    if (identity.user_id == resource.owner_user_id) {
        return true;
    }

    // ── Admin can access resources within same tenant ─────────────────────────
    auto it = std::find(identity.roles.begin(), identity.roles.end(), roles::ADMIN);
    if (it != identity.roles.end()) {
        // Admin within same tenant scope
        if (identity.tenant_id.empty() || identity.tenant_id == resource.owner_tenant_id) {
            return true;
        }
    }

    // ── Default: DENY ─────────────────────────────────────────────────────────
    // Direct object reference manipulation (e.g., changing URL from /doc/A to /doc/B)
    // is caught here — the server-side owner check always wins.
    return false;
}

void Authorization::require_permission(const common::IdentityContext& identity,
                                       const std::string& permission) {
    if (!has_permission(identity, permission)) {
        throw AuthException("PERMISSION_DENIED: missing permission: " + permission, "PERMISSION_DENIED");
    }
}

void Authorization::require_resource_access(const common::IdentityContext& identity,
                                            const ResourceContext& resource) {
    if (!can_access_resource(identity, resource)) {
        throw AuthException("PERMISSION_DENIED: resource access denied for user: " + identity.user_id, "PERMISSION_DENIED");
    }
}

} // namespace auth
