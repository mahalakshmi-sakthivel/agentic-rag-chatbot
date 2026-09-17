#include "QueryController.h"
#include "response_helpers.h"
#include "../models/QueryModels.h"
#include "../../common/uuid.h"
#include <chrono>

namespace api
{

namespace
{
// Phase 1 stub — replaced wholesale once Phase 5/6 assign a real handler.
nlohmann::json stubQueryHandler(const nlohmann::json &req, const common::IdentityContext &)
{
    std::string conversationId;
    if (req.contains("conversation_id") && req.at("conversation_id").is_string() &&
        !req.at("conversation_id").get<std::string>().empty())
    {
        conversationId = req.at("conversation_id").get<std::string>();
    }
    else
    {
        conversationId = common::generateUuid();
    }

    return nlohmann::json{
        {"query_id", common::generateUuid()},
        {"answer", "This is a placeholder response — RAG pipeline not yet connected."},
        {"sources", nlohmann::json::array()},
        {"conversation_id", conversationId},
        {"latency_ms", {{"backend_ms", 0}, {"llm_ms", 0}, {"total_ms", 0}}},
    };
}
} // namespace

QueryHandlerFn QueryController::queryHandler = stubQueryHandler;

void QueryController::handle(const drogon::HttpRequestPtr &req,
                              std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    // PHASE 2 INTEGRATION POINT: common::authenticate() is the only call
    // site that needs to change when real auth lands (see common/identity.h).
    // Not enforced yet in Phase 1 — identity is threaded through so the
    // handler (and eventually Phase 5/6) has one to consult. Once Phase 2
    // populates `authenticated`, uncomment the check below:
    //
    //   if (!identity.authenticated) {
    //       // NOTE: UNAUTHENTICATED isn't in common::ErrorCode yet — Phase 1
    //       // only owns the 5 codes in error_codes.h. Phase 2 adds it there
    //       // (Section 18.1) as part of landing this check, not before.
    //       callback(errorResponse("UNAUTHENTICATED", "Missing or invalid token"));
    //       return;
    //   }
    const auto identity = common::authenticate(req);

    const nlohmann::json parsed = nlohmann::json::parse(req->body(), nullptr, false);
    if (parsed.is_discarded())
    {
        callback(errorResponse(common::ErrorCode::VALIDATION_ERROR, "Malformed JSON body"));
        return;
    }

    const auto validation = api::query::validateQueryRequest(parsed);
    if (!validation.valid)
    {
        callback(errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                validation.errorMessage,
                                validation.errorDetails));
        return;
    }

    const auto start = std::chrono::steady_clock::now();
    nlohmann::json data = queryHandler(parsed, identity);
    const auto backendMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                std::chrono::steady_clock::now() - start)
                                .count();

    if (data.contains("latency_ms") && data.at("latency_ms").is_object())
    {
        const long long llmMs = data.at("latency_ms").value("llm_ms", 0LL);
        data["latency_ms"]["backend_ms"] = backendMs;
        data["latency_ms"]["total_ms"] = backendMs + llmMs;
    }

    callback(successResponse(data));
}

} // namespace api
