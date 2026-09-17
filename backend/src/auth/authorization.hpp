#pragma once
/**
 * @file authorization.hpp
 * @brief Server-side authorization — RBAC and resource ownership checks.
 *
 * "Authentication = Who are you?"
 * "Authorization  = What are you allowed to do?"
 *
 * All checks run on the SERVER using the validated IdentityContext.
 * The frontend CANNOT override these decisions.
 *
 * Fail closed: missing/unknown permissions → DENIED.
 */

#include "identity_context.hpp"
#include "roles.hpp"

#include <string>

namespace auth {

/**
 * @brief Exception thrown when an authorization or authentication check fails.
 */
class AuthException : public std::runtime_error {
public:
    explicit AuthException(const std::string& msg, const std::string& code = "PERMISSION_DENIED")
        : std::runtime_error(msg), code_(code) {}

    const std::string& code() const noexcept { return code_; }

private:
    std::string code_;
};

/**
 * @brief Describes a resource that authorization checks run against.
 */
struct ResourceContext {
    std::string resource_id;
    std::string owner_user_id;   ///< user_id of the resource owner
    std::string owner_tenant_id; ///< tenant scope of the resource
    std::string resource_type;   ///< e.g., "document", "session", "conversation"
};

/**
 * @brief Authorization service — evaluates permissions and ownership.
 *
 * Inject this into handlers that need authorization checks.
 * Do NOT bypass this class — never call the DB directly for permission checks.
 */
class Authorization {
public:
    /**
     * @brief Check if an identity has a specific permission.
     *
     * Permission is determined by the user's ROLES, not client-supplied claims.
     *
     * @param identity    Validated IdentityContext from authentication middleware.
     * @param permission  One of the permissions:: constants.
     * @return true if allowed, false if denied (fail closed).
     */
    static bool has_permission(const IdentityContext& identity,
                               const std::string& permission) noexcept;

    /**
     * @brief Check if an identity can access a specific resource.
     *
     * Rules:
     *   - User can access their own resources (owner_user_id == identity.user_id)
     *   - Admin can access resources within the same tenant
     *   - Tenant scope is always enforced (prevents cross-tenant access)
     *   - A user changing the resource ID in a URL does NOT bypass this check
     *
     * @param identity  Validated IdentityContext.
     * @param resource  Resource descriptor with verified owner information.
     * @return true if access is allowed, false otherwise.
     */
    static bool can_access_resource(const IdentityContext& identity,
                                    const ResourceContext& resource) noexcept;

    /**
     * @brief Require a permission — throws std::runtime_error if denied.
     *
     * Use in handlers where you want an exception instead of a bool.
     * The middleware will catch this and return HTTP 403.
     */
    static void require_permission(const IdentityContext& identity,
                                   const std::string& permission);

    /**
     * @brief Require resource access — throws std::runtime_error if denied.
     */
    static void require_resource_access(const IdentityContext& identity,
                                        const ResourceContext& resource);

private:
    Authorization() = delete;
};

} // namespace auth
