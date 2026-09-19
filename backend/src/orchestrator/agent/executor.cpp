#include "executor.h"

namespace orchestrator {

Executor::Executor(std::shared_ptr<ToolRegistry> registry, AgentLimits limits)
    : registry_(registry), limits_(limits) {}

bool Executor::execute_step(AgentState& state) {
    if (state.current_step >= state.plan.size()) {
        return false; // No more steps
    }

    if (state.total_tool_calls >= limits_.max_total_tool_calls) {
        ToolResult limit_err;
        limit_err.tool_name = "executor";
        limit_err.status = "failed";
        limit_err.error = {
            {"code", "AGENT_EXECUTION_LIMIT"},
            {"message", "Maximum agent execution limit reached."}
        };
        state.tool_results.push_back(limit_err);
        state.current_step++;
        return false;
    }

    const auto& step = state.plan[state.current_step];
    auto tool = registry_->get_tool(step.action);

    if (!tool) {
        ToolResult not_found_err;
        not_found_err.tool_name = step.action;
        not_found_err.status = "failed";
        not_found_err.error = {
            {"code", "TOOL_NOT_FOUND"},
            {"message", "Unregistered tool requested."}
        };
        state.tool_results.push_back(not_found_err);
        state.current_step++;
        return false; // Stop on error
    }

    // Build input based on tool
    nlohmann::json input;
    if (step.action == "vector_search") {
        input["query_text"] = state.normalized_query;
        input["top_k"] = limits_.max_top_k;
    } else if (step.action == "structured_query") {
        input["query"] = state.normalized_query;
    } else if (step.action == "calculator") {
        // mock expression
        input["expression"] = "1250000 / 100";
    }

    ToolResult result = tool->execute(input, state.identity);
    state.total_tool_calls++;
    state.execution_time_ms += result.execution_ms;
    
    if (step.action == "vector_search" || step.action == "structured_query") {
        state.retrieval_attempts++;
    }

    if (result.status == "success" && !result.sources.is_null()) {
        for (const auto& chunk : result.sources) {
            state.retrieved_chunks.push_back(chunk);
        }
    }

    state.tool_results.push_back(result);
    state.current_step++;
    
    return result.status == "success";
}

} // namespace orchestrator
