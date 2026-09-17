/**
 * @file roles.cpp
 */

#include "roles.hpp"

namespace auth {

std::unordered_set<std::string> permissions_for_role(const std::string& role) {
    if (role == roles::USER) {
        return {
            permissions::LOGIN,
            permissions::LOGOUT,
            permissions::VIEW_PROFILE,
            permissions::USE_CHATBOT,
            permissions::VIEW_OWN_SESSIONS,
            permissions::DELETE_OWN_SESSION,
            permissions::VIEW_OWN_DOCUMENTS,
            permissions::UPLOAD_DOCUMENTS,
        };
    }
    if (role == roles::ADMIN) {
        return {
            // Admins inherit all user permissions
            permissions::LOGIN,
            permissions::LOGOUT,
            permissions::VIEW_PROFILE,
            permissions::USE_CHATBOT,
            permissions::VIEW_OWN_SESSIONS,
            permissions::DELETE_OWN_SESSION,
            permissions::VIEW_OWN_DOCUMENTS,
            permissions::UPLOAD_DOCUMENTS,
            // Admin-only
            permissions::MANAGE_USERS,
            permissions::VIEW_ALL_SESSIONS,
            permissions::ADMIN_CONFIG,
        };
    }
    // Unknown role → no permissions (fail closed)
    return {};
}

std::unordered_set<std::string> permissions_for_roles(
    const std::vector<std::string>& roles_list)
{
    std::unordered_set<std::string> combined;
    for (const auto& r : roles_list) {
        auto perms = permissions_for_role(r);
        combined.insert(perms.begin(), perms.end());
    }
    return combined;
}

} // namespace auth
