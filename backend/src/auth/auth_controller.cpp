/**
 * @file auth_controller.cpp
 *
 * REST handlers for the authentication API.
 * Uses Crow HTTP framework.
 *
 * All responses follow §19 status codes.
 * Error messages are generic — they do not reveal sensitive implementation details.
 */

#include "auth_controller.hpp"
#include "authorization.hpp"
#include "roles.hpp"

#include <crow.h>
#include <stdexcept>
#include <iostream>

namespace auth {

AuthController::AuthController(AuthService& auth_service,
                               SessionManager& session_manager)
    : auth_service_(auth_service)
    , session_manager_(session_manager)
{}

void AuthController::register_routes(crow::SimpleApp& app) {
    // POST /auth/register
    CROW_ROUTE(app, "/auth/register").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) {
        return handle_register(req);
    });

    // POST /auth/login
    CROW_ROUTE(app, "/auth/login").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) {
        return handle_login(req);
    });

    // POST /auth/logout
    CROW_ROUTE(app, "/auth/logout").methods(crow::HTTPMethod::POST)
    ([this](const crow::request& req) {
        return handle_logout(req);
    });

    // GET /auth/me
    CROW_ROUTE(app, "/auth/me").methods(crow::HTTPMethod::GET)
    ([this](const crow::request& req) {
        return handle_me(req);
    });

    // GET /sessions
    CROW_ROUTE(app, "/sessions").methods(crow::HTTPMethod::GET)
    ([this](const crow::request& req) {
        return handle_list_sessions(req);
    });

    // DELETE /sessions/{id}
    CROW_ROUTE(app, "/sessions/<string>").methods(crow::HTTPMethod::DELETE)
    ([this](const crow::request& req, const std::string& session_id) {
        return handle_delete_session(req, session_id);
    });
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /auth/register
// ─────────────────────────────────────────────────────────────────────────────

crow::response AuthController::handle_register(const crow::request& req) {
    try {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email") || !body.has("password")) {
            return error_response(400, "email and password are required");
        }

        RegisterRequest reg_req;
        reg_req.email     = body["email"].s();
        reg_req.password  = body["password"].s();
        if (body.has("tenant_id")) reg_req.tenant_id = body["tenant_id"].s();

        // NOTE: password is NOT logged beyond this point
        auto result = auth_service_.register_user(reg_req);

        if (!result.success) {
            return error_response(400, result.message);
        }

        crow::json::wvalue resp;
        resp["user_id"] = result.user_id;
        resp["message"] = result.message;
        return json_response(201, resp);

    } catch (const std::exception& e) {
        std::cerr << "[auth_controller] register error: " << e.what() << "\n";
        return error_response(500, "Internal server error");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /auth/login
// ─────────────────────────────────────────────────────────────────────────────

crow::response AuthController::handle_login(const crow::request& req) {
    try {
        auto body = crow::json::load(req.body);
        if (!body || !body.has("email") || !body.has("password")) {
            return error_response(400, "email and password are required");
        }

        LoginRequest login_req;
        login_req.email    = body["email"].s();
        login_req.password = body["password"].s();
        if (body.has("tenant_id")) login_req.tenant_id = body["tenant_id"].s();

        auto result = auth_service_.login(login_req);

        if (!result.success) {
            // Return 401 for invalid credentials — do NOT reveal reason
            return error_response(401, result.message);
        }

        // NOTE: access_token is returned to client but NOT logged
        crow::json::wvalue resp;
        resp["access_token"] = result.access_token;
        resp["token_type"]   = result.token_type;
        resp["expires_in"]   = result.expires_in;
        return json_response(200, resp);

    } catch (const std::exception& e) {
        std::cerr << "[auth_controller] login error: " << e.what() << "\n";
        return error_response(500, "Internal server error");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// POST /auth/logout
// ─────────────────────────────────────────────────────────────────────────────

crow::response AuthController::handle_logout(const crow::request& req) {
    try {
        auto identity = extract_identity(req);
        if (!identity.is_valid()) {
            return error_response(401, "Authentication required");
        }

        auth_service_.logout(identity);

        crow::json::wvalue resp;
        resp["message"] = "Logged out successfully";
        return json_response(200, resp);

    } catch (const std::exception& e) {
        std::cerr << "[auth_controller] logout error: " << e.what() << "\n";
        return error_response(500, "Internal server error");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /auth/me
// ─────────────────────────────────────────────────────────────────────────────

crow::response AuthController::handle_me(const crow::request& req) {
    try {
        auto identity = extract_identity(req);
        if (!identity.is_valid()) {
            return error_response(401, "Authentication required");
        }

        auto user = auth_service_.get_current_user(identity);
        if (!user.has_value()) {
            return error_response(404, "User not found");
        }

        crow::json::wvalue resp;
        resp["user_id"]   = user->user_id;
        resp["email"]     = user->email;
        resp["tenant_id"] = user->tenant_id;
        resp["roles"]     = user->roles;
        // NOTE: password hash is NEVER included in this response
        return json_response(200, resp);

    } catch (const std::exception& e) {
        std::cerr << "[auth_controller] me error: " << e.what() << "\n";
        return error_response(500, "Internal server error");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// GET /sessions
// ─────────────────────────────────────────────────────────────────────────────

crow::response AuthController::handle_list_sessions(const crow::request& req) {
    try {
        auto identity = extract_identity(req);
        if (!identity.is_valid()) {
            return error_response(401, "Authentication required");
        }

        // Authorization: users can only list their own sessions
        Authorization::require_permission(identity, permissions::VIEW_OWN_SESSIONS);

        auto sessions = session_manager_.list_sessions(identity.user_id);

        crow::json::wvalue resp;
        crow::json::wvalue::list session_list;
        for (const auto& s : sessions) {
            crow::json::wvalue entry;
            entry["id"]      = s.id;
            entry["status"]  = (s.status == SessionStatus::ACTIVE)   ? "active"   :
                               (s.status == SessionStatus::REVOKED)  ? "revoked"  : "expired";
            session_list.push_back(std::move(entry));
        }
        resp["sessions"] = std::move(session_list);
        return json_response(200, resp);

    } catch (const std::runtime_error& e) {
        return error_response(403, "Forbidden");
    } catch (const std::exception& e) {
        std::cerr << "[auth_controller] list_sessions error: " << e.what() << "\n";
        return error_response(500, "Internal server error");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// DELETE /sessions/{id}
// ─────────────────────────────────────────────────────────────────────────────

crow::response AuthController::handle_delete_session(const crow::request& req,
                                                      const std::string& session_id) {
    try {
        auto identity = extract_identity(req);
        if (!identity.is_valid()) {
            return error_response(401, "Authentication required");
        }

        Authorization::require_permission(identity, permissions::DELETE_OWN_SESSION);

        // Ownership is enforced inside delete_session — user can only delete own sessions
        bool deleted = session_manager_.delete_session(session_id, identity.user_id);
        if (!deleted) {
            return error_response(404, "Session not found");
        }

        crow::json::wvalue resp;
        resp["message"] = "Session deleted";
        return json_response(200, resp);

    } catch (const std::runtime_error& e) {
        return error_response(403, "Forbidden");
    } catch (const std::exception& e) {
        std::cerr << "[auth_controller] delete_session error: " << e.what() << "\n";
        return error_response(500, "Internal server error");
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

IdentityContext AuthController::extract_identity(const crow::request& req) {
    // The identity was attached by AuthenticationMiddleware
    // In Crow, middleware stores context in the request context map
    // Retrieve the pre-validated identity — do NOT validate the token here again
    auto* ctx = req.get_header_value("X-Identity-User-Id");
    if (!ctx || std::string(ctx).empty()) {
        return IdentityContext::unauthenticated();
    }
    // Identity was set by middleware
    IdentityContext identity;
    identity.user_id    = req.get_header_value("X-Identity-User-Id");
    identity.tenant_id  = req.get_header_value("X-Identity-Tenant-Id");
    identity.session_id = req.get_header_value("X-Identity-Session-Id");

    std::string roles_str = req.get_header_value("X-Identity-Roles");
    std::istringstream ss(roles_str);
    std::string role;
    while (std::getline(ss, role, ',')) {
        if (!role.empty()) identity.roles.push_back(role);
    }
    return identity;
}

crow::response AuthController::error_response(int status, const std::string& message) {
    crow::json::wvalue body;
    body["error"]  = message;
    body["status"] = status;
    auto resp = crow::response(status, body.dump());
    resp.set_header("Content-Type", "application/json");
    return resp;
}

crow::response AuthController::json_response(int status, const crow::json::wvalue& body) {
    auto resp = crow::response(status, body.dump());
    resp.set_header("Content-Type", "application/json");
    return resp;
}

} // namespace auth
