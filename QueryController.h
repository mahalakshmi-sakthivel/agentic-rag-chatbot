// api/controllers/QueryController.h
//
// Section 7.2 — POST /v1/query. Stubbed in Phase 1: validates the request
// shape and returns a placeholder answer with the FINAL field names so
// Phases 5/6 can slot real retrieval/LLM logic in without changing this
// response shape later.
//
#pragma once

#include "../../common/identity.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>
#include <nlohmann/json.hpp>

namespace api
{

// SEAM FOR PHASE 5/6 (Agentic RAG / LLM Integration): swap this function
// pointer for the real orchestration + LLM call. The returned JSON must keep
// the Section 7.2 field names (query_id, answer, sources, conversation_id,
// latency_ms) — Phase 8 (UI) builds against that exact shape. `backend_ms`
// is overwritten by QueryController after this returns, so the seam only
// needs to fill `llm_ms` correctly once it does real work.
using QueryHandlerFn = std::function<nlohmann::json(
    const nlohmann::json &validatedRequest, const common::IdentityContext &identity)>;

class QueryController
{
public:
    static void handle(const drogon::HttpRequestPtr &req,
                        std::function<void(const drogon::HttpResponsePtr &)> &&callback);

    // Defaults to the Phase 1 stub below. Later phases reassign this (e.g.
    // from main.cpp at startup) — no controller code changes required.
    static QueryHandlerFn queryHandler;
};

} // namespace api
