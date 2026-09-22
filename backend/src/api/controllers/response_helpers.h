// api/controllers/response_helpers.h
//
// Tiny Drogon-specific glue shared by all controllers so the JSON envelope
// from common/envelope.h doesn't get re-wrapped into an HttpResponse three
// slightly-different ways.
//
#pragma once

#include "../../common/envelope.h"
#include "../../common/error_codes.h"
#include "../../common/uuid.h"
#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

namespace api
{

inline drogon::HttpResponsePtr jsonResponse(const nlohmann::json &body,
                                             drogon::HttpStatusCode status)
{
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(status);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(body.dump());
    return resp;
}

inline drogon::HttpResponsePtr successResponse(const nlohmann::json &data,
                                                 drogon::HttpStatusCode status = drogon::k200OK)
{
    const std::string requestId = common::generateUuid();
    return jsonResponse(common::makeSuccessEnvelope(data, requestId), status);
}

inline drogon::HttpResponsePtr errorResponse(const std::string &code,
                                              const std::string &message,
                                              const nlohmann::json &details = nlohmann::json::object())
{
    const std::string requestId = common::generateUuid();
    const auto status = static_cast<drogon::HttpStatusCode>(common::httpStatusForErrorCode(code));
    return jsonResponse(common::makeErrorEnvelope(code, message, details, requestId), status);
}

} // namespace api
