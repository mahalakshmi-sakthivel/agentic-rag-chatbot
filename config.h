// common/config.h
//
// Loads the variables Phase 1 owns (PHASE_1_CONTRACT.md Section 13) from the
// environment. Never hardcode secrets/paths in controller code — read them
// through AppConfig instead.
//
#pragma once

#include <cstdlib>
#include <string>

namespace common
{

struct AppConfig
{
    int backendPort = 8080;
    std::string fileStoragePath = "./storage/uploads";
    std::string logLevel = "info";
    std::string environment = "development";

    // Open Decision #15 (parent Section 31) is still TBD — defaulted to
    // 50MB here and overridable via MAX_UPLOAD_SIZE_MB until the team
    // confirms a final value.
    std::size_t maxUploadSizeBytes = 50ull * 1024 * 1024;

    static AppConfig fromEnvironment()
    {
        AppConfig cfg;

        cfg.backendPort = readInt("BACKEND_PORT", cfg.backendPort);
        cfg.fileStoragePath = readString("FILE_STORAGE_PATH", cfg.fileStoragePath);
        cfg.logLevel = readString("LOG_LEVEL", cfg.logLevel);
        cfg.environment = readString("ENVIRONMENT", cfg.environment);

        const int maxSizeMb = readInt("MAX_UPLOAD_SIZE_MB", 50);
        cfg.maxUploadSizeBytes = static_cast<std::size_t>(maxSizeMb) * 1024 * 1024;

        return cfg;
    }

private:
    static std::string readString(const char *name, const std::string &fallback)
    {
        const char *value = std::getenv(name);
        return (value != nullptr && *value != '\0') ? std::string(value) : fallback;
    }

    static int readInt(const char *name, int fallback)
    {
        const char *value = std::getenv(name);
        if (value == nullptr || *value == '\0')
        {
            return fallback;
        }
        try
        {
            return std::stoi(value);
        }
        catch (...)
        {
            return fallback;
        }
    }
};

} // namespace common
