#pragma once
/**
 * @file roles.hpp
 * @brief Role definitions and permission constants for RBAC.
 *
 * The permission matrix follows §10 and §36 of PHASE_2_AUTHENTICATION_AUTHORIZATION.md
 *
 * Roles:
 *   - "user"  — Normal chatbot user
 *   - "admin" — Administrator with user-management privileges
 *
 * Permissions are fine-grained strings checked via authorization.hpp.
 */

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

namespace auth {

// ── Role name constants ────────────────────────────────────────────────────────
namespace roles {
    inline constexpr const char* USER  = "user";
    inline constexpr const char* ADMIN = "admin";
} // namespace roles

// ── Permission name constants ──────────────────────────────────────────────────
namespace permissions {
    // Auth
    inline constexpr const char* LOGIN          = "auth:login";
    inline constexpr const char* LOGOUT         = "auth:logout";
    inline constexpr const char* VIEW_PROFILE   = "auth:view_profile";

    // Chatbot
    inline constexpr const char* USE_CHATBOT    = "chat:use";

    // Sessions
    inline constexpr const char* VIEW_OWN_SESSIONS    = "sessions:view_own";
    inline constexpr const char* DELETE_OWN_SESSION   = "sessions:delete_own";

    // Documents / files
    inline constexpr const char* VIEW_OWN_DOCUMENTS   = "docs:view_own";
    inline constexpr const char* UPLOAD_DOCUMENTS     = "docs:upload";

    // Admin-only
    inline constexpr const char* MANAGE_USERS         = "admin:manage_users";
    inline constexpr const char* VIEW_ALL_SESSIONS    = "admin:view_all_sessions";
    inline constexpr const char* ADMIN_CONFIG         = "admin:config";
} // namespace permissions

/**
 * @brief Returns the set of permissions granted to a given role.
 * @param role One of roles::USER or roles::ADMIN.
 * @return Set of permission strings.
 */
std::unordered_set<std::string> permissions_for_role(const std::string& role);

/**
 * @brief Returns all permissions for a list of roles (union).
 */
std::unordered_set<std::string> permissions_for_roles(
    const std::vector<std::string>& roles_list);

} // namespace auth
