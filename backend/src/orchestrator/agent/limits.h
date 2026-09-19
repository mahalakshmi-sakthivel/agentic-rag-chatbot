#pragma once

namespace orchestrator {

struct AgentLimits {
    int max_retrieval_attempts = 2;
    int max_total_tool_calls = 5;
    int max_plan_steps = 8;
    int max_clarification_attempts = 1;
    int max_top_k = 20;
    long long max_execution_time_ms = 1000;
};

} // namespace orchestrator
