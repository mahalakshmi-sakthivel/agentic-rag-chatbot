#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "../../common/identity.h"

namespace orchestrator {

struct StepSummary {
    int step;
    std::string tool;
    std::string result_summary;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StepSummary, step, tool, result_summary)
};

// Orchestrator Context Object (Phase 5 -> Phase 6)
struct OrchestratorContext {
    std::string query_id;
    common::IdentityContext identity;
    std::string original_query;
    std::vector<nlohmann::json> retrieved_chunks;
    
    // Optional fields
    std::optional<std::string> refined_query;
    std::vector<StepSummary> steps_taken;
    std::vector<nlohmann::json> conversation_history;

    // Clarification Handling
    bool needs_clarification = false;
    std::optional<std::string> clarification_prompt;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["query_id"] = query_id;
        j["identity"] = identity;
        j["original_query"] = original_query;
        j["retrieved_chunks"] = retrieved_chunks;

        if (refined_query) {
            j["refined_query"] = *refined_query;
        }
        if (!steps_taken.empty()) {
            j["steps_taken"] = steps_taken;
        }
        if (!conversation_history.empty()) {
            j["conversation_history"] = conversation_history;
        }
        
        j["needs_clarification"] = needs_clarification;
        if (clarification_prompt) {
            j["clarification_prompt"] = *clarification_prompt;
        }

        return j;
    }
};

} // namespace orchestrator
