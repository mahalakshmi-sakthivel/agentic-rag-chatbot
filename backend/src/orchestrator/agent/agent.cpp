#include "agent.h"

namespace orchestrator {

Agent::Agent(std::shared_ptr<ToolRegistry> registry)
    : registry_(registry) {}

OrchestratorContext Agent::process_query(
    const std::string& query, 
    const common::IdentityContext& identity, 
    const std::string& query_id) {
    
    AgentState state;
    state.query_id = query_id;
    state.identity = identity;
    state.original_query = query;
    state.normalized_query = query; // Simple normalization for now

    state.intent = intent_detector_.analyze(state.normalized_query);
    
    if (state.intent == IntentType::CLARIFICATION_REQUIRED) {
        state.needs_clarification = true;
        state.clarification_prompt = "Could you specify your question more clearly?";
        return context_builder_.build(state);
    }

    state.plan = planner_.create_plan(state);
    
    Executor executor(registry_);
    
    // Execution Loop
    while (state.current_step < state.plan.size()) {
        bool success = executor.execute_step(state);
        if (!success) {
            // Check if we hit limits or errors
            break;
        }
    }

    if (!evaluator_.is_sufficient(state)) {
        // Query refinement logic goes here in a real implementation
        // For now we just mark as needing clarification
        state.needs_clarification = true;
    }

    return context_builder_.build(state);
}

} // namespace orchestrator
