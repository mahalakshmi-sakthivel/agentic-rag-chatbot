#include "router.h"
#include "controllers/HealthController.h"
#include "controllers/QueryController.h"
#include "controllers/UploadController.h"
#include "controllers/response_helpers.h"
#include "../auth/auth_controller.hpp"
#include <memory>

namespace api
{

void registerRoutes(drogon::HttpAppFramework &app, const common::AppConfig &config)
{
    // Section 7.1 — GET /v1/health (no auth)
    app.registerHandler("/v1/health", &HealthController::handle, {drogon::Get});

    // Section 7.2 — POST /v1/query (stub; auth threaded through but not enforced yet)
    app.registerHandler("/v1/query", &QueryController::handle, {drogon::Post});

    // Section 7.3 — POST /v1/data/upload (real receive + storage)
    auto uploadController = std::make_shared<UploadController>(config);
    app.registerHandler(
        "/v1/data/upload",
        [uploadController](const drogon::HttpRequestPtr &req,
                            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            uploadController->handle(req, std::move(callback));
        },
        {drogon::Post});

    // Phase 2 — Auth endpoints (§ PHASE_2_AUTHENTICATION_AUTHORIZATION.md)
    // AuthController inherits drogon::HttpController<AuthController> and registers
    // its routes via METHOD_LIST_BEGIN/METHOD_ADD static initializers, which run
    // automatically when the translation unit is linked.
    //
    // The three routes registered by AuthController are:
    //   POST /v1/auth/login   — Exchange credentials for a JWT access token
    //   POST /v1/auth/refresh — Refresh an active session with a new token
    //   POST /v1/auth/logout  — Revoke the current session (immediate invalidation)
    //
    // Note: auth::AuthController::init() is called in main.cpp BEFORE app.run()
    // to inject the AuthService and SessionManager dependencies.
    (void)auth::AuthController::init; // ensures the symbol is referenced/linked

    // Section 9 — unmatched routes still return the standard envelope, not
    // Drogon's default HTML 404 page.
    // NOTE: Drogon's setCustom404Page takes a pre-built response, so the
    // request_id/timestamp in its meta are generated once at startup rather
    // than per-request. Acceptable for Phase 1; revisit if that becomes a
    // problem for audit logging in Section 20 of the parent contract.
    app.setCustom404Page(errorResponse(common::ErrorCode::NOT_FOUND, "Route not found"));
}

} // namespace api
