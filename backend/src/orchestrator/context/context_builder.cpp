#include "context_builder.h"

namespace orchestrator {

OrchestratorContext ContextBuilder::build(const AgentState& state) const {
    OrchestratorContext ctx;
    ctx.query_id = state.query_id;
    ctx.identity = state.identity;
    ctx.original_query = state.original_query;
    ctx.retrieved_chunks = state.retrieved_chunks;

    if (state.original_query != state.normalized_query) {
        ctx.refined_query = state.normalized_query;
    }

    if (state.needs_clarification || state.retrieved_chunks.empty() && 
        (state.intent == IntentType::DOCUMENT_LOOKUP || state.intent == IntentType::STRUCTURED_DATA_QUERY)) {
        ctx.needs_clarification = true;
        ctx.clarification_prompt = state.clarification_prompt.value_or("Could you please provide more clarification? I don't have enough information.");
    }

    for (size_t i = 0; i < state.tool_results.size(); ++i) {
        const auto& tr = state.tool_results[i];
        ctx.steps_taken.push_back({
            static_cast<int>(i + 1),
            tr.tool_name,
            tr.status == "success" ? "Operation succeeded" : "Operation failed"
        });
    }

    return ctx;
}

} // namespace orchestrator
