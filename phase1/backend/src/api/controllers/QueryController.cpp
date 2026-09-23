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
    // PHASE 2 INTEGRATION POINT: common::authenticate() delegates to a
    // swappable verifier (see common/identity.h). Real enforcement below —
    // this is no longer a no-op. Today's default verifier is a documented
    // dev-only placeholder, NOT real JWT verification; Phase 2 swaps in the
    // real one via common::setTokenVerifier() at startup, with zero changes
    // needed here.
    const auto identity = common::authenticate(req);
    if (!identity.authenticated)
    {
        callback(errorResponse(common::ErrorCode::UNAUTHENTICATED,
                                "Missing or invalid authentication token"));
        return;
    }

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

    // Checklist item 4 — pass the normalized (trimmed) body downstream, not
    // the raw parsed body, so the stub/real handler never sees untrimmed
    // whitespace that validation already decided to ignore.
    const auto start = std::chrono::steady_clock::now();
    nlohmann::json data = queryHandler(validation.normalizedBody, identity);
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
