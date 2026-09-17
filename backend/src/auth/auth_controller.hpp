#pragma once
/**
 * @file auth_controller.hpp
 * @brief REST endpoint handlers for authentication API.
 *
 * Endpoints (§18 of phase doc):
 *   POST /auth/register    — Create new account
 *   POST /auth/login       — Authenticate and receive token
 *   POST /auth/logout      — Invalidate current session
 *   GET  /auth/me          — Return authenticated user's profile
 *   GET  /sessions         — List own sessions
 *   DELETE /sessions/{id}  — Delete a specific session
 *
 * HTTP status codes (§19):
 *   200 OK, 201 Created, 400 Bad Request, 401 Unauthorized, 403 Forbidden,
 *   404 Not Found, 500 Internal Server Error
 *
 * Error responses do NOT reveal sensitive information.
 */

#include "auth_service.hpp"
#include "session_manager.hpp"
#include "identity_context.hpp"

#include <crow.h>
#include <string>

namespace auth {

class AuthController {
public:
    explicit AuthController(AuthService& auth_service, SessionManager& session_manager);

    // ── Register the routes on a Crow app ─────────────────────────────────────
    void register_routes(crow::SimpleApp& app);

private:
    AuthService&    auth_service_;
    SessionManager& session_manager_;

    // ── Handlers ──────────────────────────────────────────────────────────────
    crow::response handle_register(const crow::request& req);
    crow::response handle_login(const crow::request& req);
    crow::response handle_logout(const crow::request& req);
    crow::response handle_me(const crow::request& req);
    crow::response handle_list_sessions(const crow::request& req);
    crow::response handle_delete_session(const crow::request& req,
                                         const std::string& session_id);

    // ── Helpers ───────────────────────────────────────────────────────────────
    static IdentityContext extract_identity(const crow::request& req);
    static crow::response error_response(int status, const std::string& message);
    static crow::response json_response(int status, const crow::json::wvalue& body);
};

} // namespace auth
