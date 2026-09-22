// main.cpp
//
// Phase 1 entry point. Loads AppConfig from the environment, registers the
// three Phase 1 routes, and starts Drogon. Deliberately thin — all real
// logic lives in api/controllers/*.
//
#include "api/router.h"
#include "common/config.h"
#include "common/identity.h"

// Phase 2 Modules
#include "auth_config.hpp"
#include "db/database.hpp"
#include "auth/auth_service.hpp"
#include "auth/session_manager.hpp"
#include "auth/token_service.hpp"
#include "auth/jwt_verifier.hpp"
#include "auth/auth_controller.hpp"

#include <drogon/drogon.h>
#include <cstdlib>
#include <iostream>
#include <thread>

namespace
{

trantor::Logger::LogLevel parseLogLevel(const std::string &level)
{
    if (level == "debug") return trantor::Logger::kDebug;
    if (level == "warn") return trantor::Logger::kWarn;
    if (level == "error") return trantor::Logger::kError;
    return trantor::Logger::kInfo;
}

} // namespace

int main()
{
    const common::AppConfig config = common::AppConfig::fromEnvironment();

    trantor::Logger::setLogLevel(parseLogLevel(config.logLevel));

    LOG_INFO << "Starting agentic-rag-backend (Phase 1) "
             << "env=" << config.environment
             << " port=" << config.backendPort
             << " storage=" << config.fileStoragePath
             << " max_upload_mb=" << (config.maxUploadSizeBytes / (1024 * 1024))
             << " auth_dev_bypass=" << (config.authDevBypassEnabled ? "true" : "false");

    if (config.authDevBypassEnabled && config.environment != "development")
    {
        LOG_WARN << "AUTH_DEV_BYPASS is true in a non-development environment "
                    "(ENVIRONMENT=" << config.environment << "). This must be "
                    "false outside local dev (Section 19.1) — fix before deploying.";
    }

    // ---------------------------------------------------------------------
    // PHASE 2 INTEGRATION POINT
    // ---------------------------------------------------------------------
    auth::AuthConfig auth_cfg;
    try {
        auth_cfg = auth::AuthConfig::from_env();
    } catch (const std::exception& e) {
        LOG_ERROR << "Auth configuration error: " << e.what();
        return 1;
    }

    auto db = std::make_shared<auth::Database>(auth_cfg.db_path);
    try {
        db->run_migrations("db/migrations");
    } catch (const std::exception& e) {
        LOG_ERROR << "Migration error: " << e.what();
        return 1;
    }

    auto auth_svc = std::make_shared<auth::AuthService>(*db, auth_cfg);
    auto session_mgr = std::make_shared<auth::SessionManager>(*db);
    auto token_svc = std::make_shared<auth::TokenService>(auth_cfg);

    // Initialize the token verifier
    auto jwt_verifier = std::make_shared<auth::JwtVerifier>(token_svc, session_mgr);
    common::setTokenVerifier([jwt_verifier](const drogon::HttpRequestPtr& req) {
        return jwt_verifier->verify(req);
    });

    // Initialize AuthController dependencies
    auth::AuthController::init(auth_svc, session_mgr);
    
    auto &app = drogon::app();
    api::registerRoutes(app, config);

    app.addListener("0.0.0.0", config.backendPort);

    // Section 12: async-capable framework, single I/O loop is sufficient for
    // Phase 1's stub workload — revisit thread count once Phase 4/5/6 add
    // real retrieval/LLM latency.
    app.setThreadNum(std::thread::hardware_concurrency() > 0
                          ? std::thread::hardware_concurrency()
                          : 2);

    app.run();
    return 0;
}
