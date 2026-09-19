#include "query_refiner.h"
#include <iostream>

namespace orchestrator {

bool QueryRefiner::refine(AgentState& state) const {
    if (state.retrieval_attempts >= 2) {
        return false; // Limit reached
    }

    if (state.tool_results.empty()) {
        return false;
    }

    // Very simple mock logic for refinement
    if (state.normalized_query == "revenue") {
        state.normalized_query = "Q3 total revenue 2025";
    } else {
        state.normalized_query += " detailed";
    }

    // We add another step for retrieval to the plan
    if (state.intent == IntentType::DOCUMENT_LOOKUP) {
        state.plan.push_back({static_cast<int>(state.plan.size()) + 1, "vector_search", "Refined search"});
    } else if (state.intent == IntentType::STRUCTURED_DATA_QUERY) {
        state.plan.push_back({static_cast<int>(state.plan.size()) + 1, "structured_query", "Refined search"});
    }

    return true;
}

} // namespace orchestrator
