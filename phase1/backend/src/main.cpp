// main.cpp
//
// Phase 1 entry point. Loads AppConfig from the environment, registers the
// three Phase 1 routes, and starts Drogon. Deliberately thin — all real
// logic lives in api/controllers/*.
//
#include "api/router.h"
#include "common/config.h"
#include "common/identity.h"

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
    // PHASE 2 INTEGRATION POINT — the actual "connect the wire" step.
    // Once src/auth/ exists with a real JWT verification function matching
    // common::TokenVerifierFn, replace common::devBypassVerifier with it
    // here — this one line is the entire integration, no controller changes
    // needed:
    //
    //   #include "auth/JwtVerifier.h"   // Phase 2's header
    //   common::setTokenVerifier(auth::verifyJwtBearerToken);
    //
    // Until that line is added, every request is checked against the
    // dev-only bypass verifier documented in common/identity.h.
    // ---------------------------------------------------------------------

    // ---------------------------------------------------------------------
    // PHASE 3 INTEGRATION POINT — same idea, for ingestion. Once src/ingestion/
    // exists with a handler matching api::IngestionHandlerFn (see the seam
    // doc comment in api/controllers/UploadController.h), assign it here —
    // one line, no controller changes needed:
    //
    //   #include "ingestion/IngestionHandler.h"   // Phase 3's header
    //   api::UploadController::ingestionHandler = ingestion::handleIngestion;
    //
    // Until that line is added, every upload gets a shape-correct
    // placeholder response (document_id + status "queued") with no file
    // actually persisted anywhere — see UploadController.cpp.
    // ---------------------------------------------------------------------

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
