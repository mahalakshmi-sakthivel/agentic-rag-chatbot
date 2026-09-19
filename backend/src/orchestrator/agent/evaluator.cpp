#include "evaluator.h"

namespace orchestrator {

bool Evaluator::is_sufficient(const AgentState& state) const {
    if (state.intent == IntentType::CLARIFICATION_REQUIRED || 
        state.intent == IntentType::NO_RETRIEVAL_REQUIRED) {
        return true;
    }

    if (state.tool_results.empty()) {
        return false;
    }

    // Check if we hit any errors
    for (const auto& res : state.tool_results) {
        if (res.status == "failed") {
            return false;
        }
    }

    // TBD Threshold check. For now, if we have chunks, it's sufficient
    return !state.retrieved_chunks.empty() || state.intent == IntentType::CALCULATION;
}

} // namespace orchestrator
