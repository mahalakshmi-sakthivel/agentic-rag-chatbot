/**
 * @file authorization.cpp
 */

#include "authorization.hpp"

#include <stdexcept>

namespace auth {

bool Authorization::has_permission(const IdentityContext& identity,
                                   const std::string& permission) noexcept {
    // ── Fail closed — unauthenticated identity has no permissions ─────────────
    if (!identity.is_valid()) return false;
    if (permission.empty())   return false;

    // ── Derive permissions from roles ─────────────────────────────────────────
    // Permissions are determined server-side from roles — never from the client
    auto perms = permissions_for_roles(identity.roles);
    return perms.count(permission) > 0;
}

bool Authorization::can_access_resource(const IdentityContext& identity,
                                         const ResourceContext& resource) noexcept {
    // ── Fail closed ───────────────────────────────────────────────────────────
    if (!identity.is_valid()) return false;

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
    if (identity.has_role(roles::ADMIN)) {
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

void Authorization::require_permission(const IdentityContext& identity,
                                       const std::string& permission) {
    if (!has_permission(identity, permission)) {
        throw std::runtime_error("FORBIDDEN: missing permission: " + permission);
    }
}

void Authorization::require_resource_access(const IdentityContext& identity,
                                            const ResourceContext& resource) {
    if (!can_access_resource(identity, resource)) {
        throw std::runtime_error("FORBIDDEN: resource access denied for user: " +
                                 identity.user_id);
    }
}

} // namespace auth
