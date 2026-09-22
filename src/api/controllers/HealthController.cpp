#include "HealthController.h"
#include "response_helpers.h"
#include <chrono>

namespace api
{

namespace
{
const auto kProcessStart = std::chrono::steady_clock::now();
constexpr const char *kServiceVersion = "0.1.0";
} // namespace

void HealthController::handle(const drogon::HttpRequestPtr &req,
                               std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    (void)req;

    const auto uptimeSeconds = std::chrono::duration_cast<std::chrono::seconds>(
                                    std::chrono::steady_clock::now() - kProcessStart)
                                    .count();

    nlohmann::json data{
        {"status", "ok"},
        {"uptime_seconds", uptimeSeconds},
        {"version", kServiceVersion},
    };

    callback(successResponse(data));
}

} // namespace api
