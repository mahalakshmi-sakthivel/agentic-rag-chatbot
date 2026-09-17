/**
 * @file identity_context.cpp
 */

#include "identity_context.hpp"
#include <algorithm>

namespace auth {

bool IdentityContext::has_role(const std::string& role) const noexcept {
    return std::find(roles.begin(), roles.end(), role) != roles.end();
}

bool IdentityContext::is_valid() const noexcept {
    return !user_id.empty();
}

IdentityContext IdentityContext::unauthenticated() noexcept {
    return IdentityContext{};
}

} // namespace auth
