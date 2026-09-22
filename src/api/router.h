// api/router.h
//
// Single place where Phase 1's three routes are wired up. Phase 8 (UI) and
// anyone else integrating with this backend should be able to read this
// file and know the entire public surface of the service.
//
#pragma once

#include "../common/config.h"
#include <drogon/HttpAppFramework.h>

namespace api
{

void registerRoutes(drogon::HttpAppFramework &app, const common::AppConfig &config);

} // namespace api
