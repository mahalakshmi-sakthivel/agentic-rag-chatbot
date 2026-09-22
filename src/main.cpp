// main.cpp
// Combined Phase 1 + Phase 2 + Phase 3 integration entry point.
#include "api/router.h"
#include "common/config.h"
#include "common/identity.h"
#include "api/controllers/UploadController.h"
#include "ingestion/IngestionHandler.h"

#if __has_include("auth/jwt_verifier.hpp")
#include "auth/jwt_verifier.hpp"
#include "auth/token_service.hpp"
#include "auth/session_manager.hpp"
#include "db/database.hpp"
#include "auth_config.hpp"
#define PHASE2_JWT_VERIFIER_AVAILABLE 1
#endif

#include <drogon/drogon.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <thread>
#include <filesystem>

namespace
{
trantor::Logger::LogLevel parseLogLevel(const std::string &level)
{
    if (level == "debug") return trantor::Logger::kDebug;
    if (level == "warn") return trantor::Logger::kWarn;
    if (level == "error") return trantor::Logger::kError;
    return trantor::Logger::kInfo;
}

bool envPresent(const char *name)
{
    const char *value = std::getenv(name);
    return value != nullptr && *value != '\0';
}
} // namespace

int main()
{
    const common::AppConfig config = common::AppConfig::fromEnvironment();
    trantor::Logger::setLogLevel(parseLogLevel(config.logLevel));

    LOG_INFO << "Starting agentic-rag-backend (Phase 1 + Phase 2 + Phase 3)"
             << " env=" << config.environment
             << " port=" << config.backendPort
             << " storage=" << config.fileStoragePath
             << " max_upload_mb=" << (config.maxUploadSizeBytes / (1024 * 1024))
             << " auth_dev_bypass=" << (config.authDevBypassEnabled ? "true" : "false");

    if (config.authDevBypassEnabled && config.environment != "development")
    {
        LOG_ERROR << "AUTH_DEV_BYPASS must be false outside local development.";
        return 1;
    }

#if defined(PHASE2_JWT_VERIFIER_AVAILABLE)
    // Prefer the real Phase 2 verifier whenever its required configuration is
    // available. The development bypass remains available only for local dev.
    const bool phase2ConfigPresent =
        envPresent("JWT_SECRET") &&
        envPresent("JWT_ISSUER") &&
        envPresent("JWT_AUDIENCE") &&
        envPresent("DB_PATH");

    if (phase2ConfigPresent)
    {
        try
        {
            auto auth_config = auth::AuthConfig::from_env();
            auto database = std::make_shared<auth::Database>(auth_config.db_path);

            const std::filesystem::path migration_dir =
                std::filesystem::path("db") / "migrations";
            if (std::filesystem::exists(migration_dir))
            {
                database->run_migrations(migration_dir.string());
            }

            auto token_service =
                std::make_shared<auth::TokenService>(auth_config);
            auto session_manager =
                std::make_shared<auth::SessionManager>(*database);
            auto jwt_verifier =
                std::make_shared<auth::JwtVerifier>(
                    token_service, session_manager);

            common::setTokenVerifier(
                [jwt_verifier](const drogon::HttpRequestPtr &req) {
                    return jwt_verifier->verify(req);
                });

            LOG_INFO << "Phase 2 JWT verifier wired and active";
        }
        catch (const std::exception &ex)
        {
            LOG_ERROR << "Phase 2 authentication initialization failed: "
                      << ex.what();
            return 1;
        }
    }
    else if (config.environment == "development" &&
             config.authDevBypassEnabled)
    {
        LOG_WARN << "Using development authentication bypass. "
                    "This is permitted only for local development.";
    }
    else
    {
        LOG_ERROR << "Real Phase 2 JWT configuration is required outside "
                     "local development.";
        return 1;
    }
#else
    if (!(config.environment == "development" &&
          config.authDevBypassEnabled))
    {
        LOG_ERROR << "Phase 2 JWT verifier is not present. "
                     "Shared/staging/production startup is refused.";
        return 1;
    }
    LOG_WARN << "Using development authentication bypass.";
#endif

    api::UploadController::ingestionHandler = ingestion::handleIngestion;

    auto &app = drogon::app();
    api::registerRoutes(app, config);
    app.addListener("0.0.0.0", config.backendPort);

    app.setThreadNum(std::thread::hardware_concurrency() > 0
                         ? std::thread::hardware_concurrency()
                         : 2);

    app.run();
    return 0;
}
