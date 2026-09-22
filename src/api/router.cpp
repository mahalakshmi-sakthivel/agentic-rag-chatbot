#include "router.h"
#include "controllers/HealthController.h"
#include "controllers/QueryController.h"
#include "controllers/UploadController.h"
#include "controllers/DataController.h"
#include "controllers/response_helpers.h"
#include <memory>


namespace api
{

void registerRoutes(drogon::HttpAppFramework &app, const common::AppConfig &config)
{
    // Section 7.1 — GET /v1/health (no auth)
    app.registerHandler("/v1/health", &HealthController::handle, {drogon::Get});

    // Section 7.2 — POST /v1/query (stub; authentication enforced)
    app.registerHandler("/v1/query", &QueryController::handle, {drogon::Post});

    // Section 7.3 — POST /v1/data/upload (real receive + validation;
    // forwards to Phase 3 via UploadController::ingestionHandler)
    auto uploadController = std::make_shared<UploadController>(config);
    app.registerHandler(
        "/v1/data/upload",
        [uploadController](const drogon::HttpRequestPtr &req,
                            std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            uploadController->handle(req, std::move(callback));
        },
        {drogon::Post});
          auto dataController = std::make_shared<DataController>();
    app.registerHandler(
        "/v1/data/{document_id}/status",
        [dataController](const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                         const std::string &documentId) {
            dataController->handleStatus(req, std::move(callback), documentId);
        },
        {drogon::Get});


    app.registerHandler(
    "/v1/data/{document_id}",
    [dataController](const drogon::HttpRequestPtr &req,
                     std::function<void(const drogon::HttpResponsePtr &)> &&callback,
                     const std::string &documentId) {
        dataController->handleDelete(req, std::move(callback), documentId);
    },
    {drogon::Delete});

    app.registerHandler(
        "/v1/data",
        [dataController](const drogon::HttpRequestPtr &req,
                         std::function<void(const drogon::HttpResponsePtr &)> &&callback) {
            dataController->handleList(req, std::move(callback));
        },
        {drogon::Get});

    // Section 9 — unmatched routes still return the standard envelope, not
    // Drogon's default HTML 404 page.
    // NOTE: Drogon's setCustom404Page takes a pre-built response, so the
    // request_id/timestamp in its meta are generated once at startup rather
    // than per-request. Acceptable for Phase 1; revisit if that becomes a
    // problem for audit logging in Section 20 of the parent contract.
    app.setCustom404Page(errorResponse(common::ErrorCode::NOT_FOUND, "Route not found"));
}

} // namespace api
