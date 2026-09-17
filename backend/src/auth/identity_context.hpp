#pragma once
/**
 * @file identity_context.hpp
 * @brief Authenticated user identity passed to all downstream phases.
 *
 * Only Phase 2 creates IdentityContext from raw authentication credentials.
 * Phases 3, 4, 5 receive this struct — never the raw token.
 *
 * Rule: user_id and tenant_id must not be modified by any downstream phase.
 */

#include <string>
#include <vector>

namespace auth {

/**
 * @brief Represents the verified identity of an authenticated request.
 *
 * Created by AuthenticationMiddleware after successful token/session validation.
 * Immutable once created — downstream phases read it, never write it.
 */
struct IdentityContext {
    std::string              user_id;    ///< Verified UUID of the authenticated user
    std::string              tenant_id;  ///< Tenant/application scope (empty = global)
    std::vector<std::string> roles;      ///< Assigned roles (e.g., "user", "admin")
    std::string              session_id; ///< Active session identifier (non-secret)

    // ── Helpers ──────────────────────────────────────────────────────────────

    /** @return true if the identity has the given role. */
    bool has_role(const std::string& role) const noexcept;

    /** @return true if the identity is valid (non-empty user_id). */
    bool is_valid() const noexcept;

    /** @return An empty/invalid identity (used as a sentinel value). */
    static IdentityContext unauthenticated() noexcept;
};

} // namespace auth
