#pragma once

#include "auth_service.hpp"
#include "session_manager.hpp"
#include "../common/identity.h"

#include <drogon/HttpController.h>
#include <memory>
#include <string>

namespace auth {

/**
 * @brief Drogon HTTP controller for authentication endpoints (Phase 2).
 *
 * Routes are registered at absolute paths via the PATH_LIST_BEGIN/METHOD_ADD
 * mechanism. Drogon auto-registers these routes when the translation unit is
 * linked — no manual app.registerHandler() calls are needed.
 *
 * Endpoints:
 *   POST /v1/auth/login   — Exchange credentials for a JWT
 *   POST /v1/auth/refresh — Refresh an active session
 *   POST /v1/auth/logout  — Revoke the current session
 */
class AuthController : public drogon::HttpController<AuthController> {
public:
    // Set the controller base path to "/" so that METHOD_ADD paths are
    // treated as absolute URLs (e.g., /v1/auth/login).
    METHOD_LIST_BEGIN
    METHOD_ADD(AuthController::login,   "/v1/auth/login",   drogon::Post);
    METHOD_ADD(AuthController::refresh, "/v1/auth/refresh", drogon::Post);
    METHOD_ADD(AuthController::logout,  "/v1/auth/logout",  drogon::Post);
    METHOD_LIST_END

    void login(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void refresh(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void logout(const drogon::HttpRequestPtr& req, std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    /**
     * @brief Inject dependencies before Drogon starts.
     * Call once in main() before app.run().
     */
    static void init(std::shared_ptr<AuthService> auth_svc, std::shared_ptr<SessionManager> session_mgr);

private:
    static std::shared_ptr<AuthService>    auth_service_;
    static std::shared_ptr<SessionManager> session_manager_;
};

} // namespace auth
