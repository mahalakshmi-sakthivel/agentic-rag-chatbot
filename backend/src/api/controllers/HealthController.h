// api/controllers/HealthController.h
//
// Section 7.1 — GET /v1/health. Real (not stubbed): no auth, must answer in
// single-digit milliseconds, used as the latency floor benchmark by later
// phases.
//
#pragma once

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

namespace api
{

class HealthController
{
public:
    static void handle(const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&callback);
};

} // namespace api
