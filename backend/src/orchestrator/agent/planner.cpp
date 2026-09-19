#include "planner.h"

namespace orchestrator {

std::vector<StepPlan> Planner::create_plan(const AgentState& state) const {
    std::vector<StepPlan> plan;

    switch (state.intent) {
        case IntentType::DOCUMENT_LOOKUP:
            plan.push_back({1, "vector_search", "Retrieve relevant document chunks"});
            break;
        case IntentType::STRUCTURED_DATA_QUERY:
            plan.push_back({1, "structured_query", "Extract structured tabular data"});
            break;
        case IntentType::CALCULATION:
            plan.push_back({1, "calculator", "Perform mathematical calculation"});
            break;
        case IntentType::COMPARISON:
            plan.push_back({1, "vector_search", "Retrieve for entity A"});
            plan.push_back({2, "vector_search", "Retrieve for entity B"});
            break;
        case IntentType::MULTI_STEP:
            plan.push_back({1, "structured_query", "Get data for part 1"});
            plan.push_back({2, "structured_query", "Get data for part 2"});
            plan.push_back({3, "calculator", "Calculate result"});
            break;
        case IntentType::CLARIFICATION_REQUIRED:
        case IntentType::NO_RETRIEVAL_REQUIRED:
            // No tool plan needed
            break;
    }

    return plan;
}

} // namespace orchestrator
