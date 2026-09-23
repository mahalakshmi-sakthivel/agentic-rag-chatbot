// common/envelope.h
//
// Team-Lead-controlled shared type (per PHASE_1_CONTRACT.md Section 4).
// Every controller must build responses through these functions — never
// construct a raw, non-enveloped JSON body by hand.
//
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace common
{

// Returns current UTC time as an ISO-8601 string, e.g. "2026-09-16T10:22:00Z"
inline std::string nowIso8601()
{
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tmUtc{};
#if defined(_WIN32)
    gmtime_s(&tmUtc, &t);
#else
    gmtime_r(&t, &tmUtc);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tmUtc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

inline nlohmann::json makeMeta(const std::string &requestId)
{
    return nlohmann::json{
        {"request_id", requestId},
        {"timestamp", nowIso8601()}};
}

// Section 6 — Success envelope
inline nlohmann::json makeSuccessEnvelope(const nlohmann::json &data,
                                           const std::string &requestId)
{
    return nlohmann::json{
        {"success", true},
        {"data", data},
        {"meta", makeMeta(requestId)}};
}

// Section 6 — Error envelope
inline nlohmann::json makeErrorEnvelope(const std::string &code,
                                         const std::string &message,
                                         const nlohmann::json &details,
                                         const std::string &requestId)
{
    return nlohmann::json{
        {"success", false},
        {"error", {{"code", code}, {"message", message}, {"details", details}}},
        {"meta", makeMeta(requestId)}};
}

// Convenience overload — no details payload
inline nlohmann::json makeErrorEnvelope(const std::string &code,
                                         const std::string &message,
                                         const std::string &requestId)
{
    return makeErrorEnvelope(code, message, nlohmann::json::object(), requestId);
}

} // namespace common
