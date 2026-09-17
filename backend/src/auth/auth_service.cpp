/**
 * @file auth_service.cpp
 */

#include "auth_service.hpp"

#include <stdexcept>
#include <iostream>  // For structured logging (not passwords/tokens)

namespace auth {

AuthService::AuthService(Database& db, const AuthConfig& cfg)
    : db_(db)
    , cfg_(cfg)
    , token_service_(cfg)
    , session_manager_(db)
{}

// ─────────────────────────────────────────────────────────────────────────────
// Register
// ─────────────────────────────────────────────────────────────────────────────

RegisterResult AuthService::register_user(const RegisterRequest& req) {
    // ── Input validation ──────────────────────────────────────────────────────
    if (req.email.empty() || req.password.empty()) {
        return {false, "", "Email and password are required"};
    }
    if (req.password.size() < 8) {
        return {false, "", "Password must be at least 8 characters"};
    }

    // ── Check for existing account ────────────────────────────────────────────
    // Note: we avoid revealing whether the email exists in the *error* path
    // but for registration we do need to tell the user the email is taken.
    auto existing = find_user_by_email(req.email);
    if (existing.has_value()) {
        return {false, "", "An account with this email already exists"};
    }

    // ── Hash password — NEVER store plain text ────────────────────────────────
    std::string password_hash;
    try {
        password_hash = PasswordHasher::hash(req.password);
    } catch (const std::exception& e) {
        // Log the technical error (not the password)
        std::cerr << "[auth] Password hashing failed: " << e.what() << "\n";
        return {false, "", "Registration failed — internal error"};
    }

    // ── Create user in DB ─────────────────────────────────────────────────────
    try {
        // Generate a UUID for the new user
        std::string user_id = db_.generate_uuid();

        db_.execute(
            "INSERT INTO users (id, email, password_hash, tenant_id, status, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, 'active', datetime('now'), datetime('now'))",
            {user_id, req.email, password_hash, req.tenant_id}
        );

        // Assign default role
        db_.execute(
            "INSERT INTO user_roles (user_id, role) VALUES (?, 'user')",
            {user_id}
        );

        // Log the event (NOT the password hash)
        std::cout << "[auth] event=registration status=success user_id=" << user_id << "\n";

        return {true, user_id, "Registration successful"};

    } catch (const std::exception& e) {
        std::cerr << "[auth] Registration DB error: " << e.what() << "\n";
        return {false, "", "Registration failed — internal error"};
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Login
// ─────────────────────────────────────────────────────────────────────────────

LoginResult AuthService::login(const LoginRequest& req) {
    static const LoginResult GENERIC_FAILURE{
        false, "", "Bearer", 0,
        "Invalid credentials"   // Generic — does NOT reveal whether email exists
    };

    if (req.email.empty() || req.password.empty()) {
        return {false, "", "Bearer", 0, "Email and password are required"};
    }

    // ── Find user ─────────────────────────────────────────────────────────────
    auto user = find_user_by_email(req.email);
    if (!user.has_value()) {
        // Run the hasher anyway to prevent timing-based email enumeration
        PasswordHasher::verify(req.password, "$2b$12$aaaaaaaaaaaaaaaaaaaaaa"
                                             "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        return GENERIC_FAILURE;
    }

    // ── Verify password — NEVER log the password ──────────────────────────────
    auto stored_hash = get_password_hash(user->user_id);
    if (!stored_hash.has_value() ||
        !PasswordHasher::verify(req.password, *stored_hash))
    {
        std::cout << "[auth] event=login status=failed user_id=" << user->user_id << "\n";
        return GENERIC_FAILURE;
    }

    // ── Create session ────────────────────────────────────────────────────────
    std::string session_id;
    try {
        session_id = session_manager_.create_session(user->user_id, cfg_.token_expiry_seconds);
    } catch (const std::exception& e) {
        std::cerr << "[auth] Session creation failed: " << e.what() << "\n";
        return {false, "", "Bearer", 0, "Login failed — internal error"};
    }

    // ── Issue JWT ─────────────────────────────────────────────────────────────
    auto roles = get_user_roles(user->user_id);
    TokenClaims claims{
        user->user_id,
        user->tenant_id,
        session_id,
        roles
    };

    std::string token;
    try {
        token = token_service_.create_token(claims);
    } catch (const std::exception& e) {
        std::cerr << "[auth] Token creation failed: " << e.what() << "\n";
        return {false, "", "Bearer", 0, "Login failed — internal error"};
    }

    // Log success (NOT the token)
    std::cout << "[auth] event=login status=success user_id=" << user->user_id
              << " session_id=" << session_id << "\n";

    return {true, token, "Bearer", cfg_.token_expiry_seconds, "Login successful"};
}

// ─────────────────────────────────────────────────────────────────────────────
// Logout
// ─────────────────────────────────────────────────────────────────────────────

bool AuthService::logout(const IdentityContext& identity) {
    if (!identity.is_valid()) return false;

    bool revoked = session_manager_.revoke_session(identity.session_id, identity.user_id);

    std::cout << "[auth] event=logout status=" << (revoked ? "success" : "failed")
              << " user_id=" << identity.user_id
              << " session_id=" << identity.session_id << "\n";

    return revoked;
}

// ─────────────────────────────────────────────────────────────────────────────
// Get current user
// ─────────────────────────────────────────────────────────────────────────────

std::optional<UserInfo> AuthService::get_current_user(const IdentityContext& identity) const {
    if (!identity.is_valid()) return std::nullopt;

    auto rows = db_.query(
        "SELECT id, email, tenant_id FROM users WHERE id = ? AND status = 'active'",
        {identity.user_id}
    );

    if (rows.empty()) return std::nullopt;

    UserInfo info;
    info.user_id   = rows[0].at("id");
    info.email     = rows[0].at("email");
    info.tenant_id = rows[0].at("tenant_id");
    info.roles     = identity.roles;
    return info;
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

std::optional<UserInfo> AuthService::find_user_by_email(const std::string& email) const {
    auto rows = db_.query(
        "SELECT id, email, tenant_id FROM users WHERE email = ? AND status = 'active'",
        {email}
    );
    if (rows.empty()) return std::nullopt;
    UserInfo u;
    u.user_id   = rows[0].at("id");
    u.email     = rows[0].at("email");
    u.tenant_id = rows[0].at("tenant_id");
    u.roles     = get_user_roles(u.user_id);
    return u;
}

std::optional<std::string> AuthService::get_password_hash(const std::string& user_id) const {
    // NOTE: result is NOT logged
    auto rows = db_.query(
        "SELECT password_hash FROM users WHERE id = ?",
        {user_id}
    );
    if (rows.empty()) return std::nullopt;
    return rows[0].at("password_hash");
}

std::vector<std::string> AuthService::get_user_roles(const std::string& user_id) const {
    auto rows = db_.query(
        "SELECT role FROM user_roles WHERE user_id = ?",
        {user_id}
    );
    std::vector<std::string> roles;
    for (auto& row : rows) {
        roles.push_back(row.at("role"));
    }
    if (roles.empty()) roles.push_back("user"); // Default role
    return roles;
}

} // namespace auth
