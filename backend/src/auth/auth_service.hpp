#pragma once
/**
 * @file auth_service.hpp
 * @brief Central authentication service — orchestrates register, login, logout.
 *
 * This is the main entry point for authentication operations.
 * It coordinates: PasswordHasher, TokenService, SessionManager, and the user
 * repository.
 *
 * The service NEVER:
 *   - Logs passwords
 *   - Returns raw tokens to downstream phases
 *   - Trusts client-supplied user_id
 */

#include "../../config/auth_config.hpp"
#include "../common/identity.h"
#include "db/database.hpp"
#include "password_hasher.hpp"
#include "session_manager.hpp"
#include "token_service.hpp"

#include <optional>
#include <string>

namespace auth {

// ── Result types
// ──────────────────────────────────────────────────────────────

struct RegisterRequest {
  std::string email;
  std::string password;  // Plain-text — hashed before storage
  std::string tenant_id; // Optional; empty = global scope
};

struct RegisterResult {
  bool success{false};
  std::string user_id;
  std::string message; // Public-safe message (no sensitive details)
};

struct LoginRequest {
  std::string email;
  std::string password; // Plain-text — verified, NEVER stored
  std::string tenant_id;
};

struct LoginResult {
  bool success{false};
  std::string access_token; // JWT — NOT logged
  std::string refresh_token;
  std::string token_type{"Bearer"};
  int expires_in{0};
  std::string message;
};

struct UserInfo {
  std::string user_id;
  std::string email;
  std::string tenant_id;
  std::vector<std::string> roles;
};

// ── Service
// ───────────────────────────────────────────────────────────────────

class AuthService {
public:
  AuthService(Database &db, const AuthConfig &cfg);

  /**
   * @brief Register a new user.
   * @param req  Registration data. Password is hashed immediately.
   * @return RegisterResult with success flag and public message.
   */
  RegisterResult register_user(const RegisterRequest &req);

  /**
   * @brief Authenticate a user and issue a JWT + session.
   * @param req  Login credentials (plain-text password verified, not stored).
   * @return LoginResult with JWT on success, generic failure message on
   * failure.
   *
   * Error messages do NOT reveal whether the email exists (prevents
   * enumeration).
   */
  LoginResult login(const LoginRequest &req);

  /**
   * @brief Refresh an active session, issuing a new token.
   * @param identity  Authenticated identity (requires a valid session).
   * @return LoginResult with a new JWT.
   */
  LoginResult refresh_token(const std::string &refresh_token);

  /**
   * @brief Logout — revoke the session associated with the identity.
   * @param identity  Authenticated identity (from middleware, not client).
   * @return true if session was revoked.
   */
  bool logout(const common::IdentityContext &identity);

  /**
   * @brief Return the authenticated user's public profile.
   * @param identity  Authenticated identity.
   * @return UserInfo without sensitive fields (no password hash, no token).
   */
  std::optional<UserInfo>
  get_current_user(const common::IdentityContext &identity) const;

private:
  Database &db_;
  AuthConfig cfg_;
  TokenService token_service_;
  SessionManager session_manager_;

  std::optional<UserInfo> find_user_by_email(const std::string &email) const;
  std::optional<std::string>
  get_password_hash(const std::string &user_id) const;
  std::vector<std::string> get_user_roles(const std::string &user_id) const;
};

} // namespace auth
