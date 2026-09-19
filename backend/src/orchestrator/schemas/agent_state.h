#pragma once

#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "../../common/identity.h"

namespace orchestrator {

enum class IntentType {
    DOCUMENT_LOOKUP,
    STRUCTURED_DATA_QUERY,
    CALCULATION,
    COMPARISON,
    MULTI_STEP,
    CLARIFICATION_REQUIRED,
    NO_RETRIEVAL_REQUIRED
};

struct StepPlan {
    int step_id;
    std::string action;
    std::string purpose;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(StepPlan, step_id, action, purpose)
};

struct ToolResult {
    std::string tool_name;
    std::string status;
    nlohmann::json input;
    nlohmann::json output;
    nlohmann::json sources;
    std::optional<nlohmann::json> error;
    long long execution_ms;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(ToolResult, tool_name, status, input, output, sources, error, execution_ms)
};

struct AgentState {
    std::string query_id;
    common::IdentityContext identity;
    std::string original_query;
    std::string normalized_query;
    
    int current_step = 0;
    std::vector<StepPlan> plan;
    std::vector<ToolResult> tool_results;
    std::vector<nlohmann::json> retrieved_chunks;
    
    bool needs_clarification = false;
    std::optional<std::string> clarification_prompt;
    
    int total_tool_calls = 0;
    int retrieval_attempts = 0;
    long long execution_time_ms = 0;
    
    IntentType intent = IntentType::DOCUMENT_LOOKUP;
};

} // namespace orchestrator
