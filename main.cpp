// main.cpp
//
// Phase 1 entry point. Loads AppConfig from the environment, registers the
// three Phase 1 routes, and starts Drogon. Deliberately thin — all real
// logic lives in api/controllers/*.
//
#include "api/router.h"
#include "common/config.h"

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
             << " max_upload_mb=" << (config.maxUploadSizeBytes / (1024 * 1024));

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
