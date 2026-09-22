#pragma once
/**
 * @file token_service.hpp
 * @brief JWT access-token creation and full validation.
 *
 * Validation checks (all required — fail closed):
 *   ✓ Signature (HMAC-SHA256)
 *   ✓ Expiration (exp claim)
 *   ✓ Issuer    (iss claim)
 *   ✓ Audience  (aud claim)
 *   ✓ Algorithm allowlist (HS256 only — rejects "none" attacks)
 *   ✓ Required claims present (sub, jti, iat)
 *
 * A token that can merely be *decoded* is NOT considered valid.
 */

#include "../../config/auth_config.hpp"
#include <random>
#include <vector>

#include <optional>
#include <string>

namespace auth {

struct TokenClaims {
  std::string user_id;
  std::string tenant_id;
  std::string session_id;
  std::vector<std::string> roles;
  std::string token_use{"access"};
};

class TokenService {
public:
  explicit TokenService(const AuthConfig &cfg);

  /**
   * @brief Create a signed JWT for an authenticated user.
   * @param claims  Verified identity to embed in the token.
   * @return Signed JWT string.
   * @throws std::runtime_error on signing failure.
   */
  std::string create_token(const TokenClaims &claims) const;
  std::string create_refresh_token(const TokenClaims &claims) const;

  /**
   * @brief Validate a raw JWT and extract its claims.
   *
   * Returns std::nullopt for ANY validation failure
   * (expired, invalid signature, wrong issuer, wrong algorithm, etc.)
   *
   * @param raw_token  The Bearer token string from the Authorization header.
   * @return Validated TokenClaims, or nullopt on failure.
   */
  std::optional<TokenClaims>
  validate_token(const std::string &raw_token) const noexcept;

private:
  AuthConfig cfg_;
};

} // namespace auth
