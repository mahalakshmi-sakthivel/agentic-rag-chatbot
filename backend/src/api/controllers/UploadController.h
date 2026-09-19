// api/controllers/UploadController.h
//
// Section 7.3 — POST /v1/data/upload. Real receive + disk storage; no file
// parsing (that's Phase 3). Needs AppConfig for storage path + size limit,
// so it's a small object rather than a static function like the other two
// controllers.
//
#pragma once

#include "../../common/config.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>

namespace api
{

class UploadController
{
public:
    explicit UploadController(common::AppConfig config);

    void handle(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback);

private:
    common::AppConfig config_;
};

} // namespace api
