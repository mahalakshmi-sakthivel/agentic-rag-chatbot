#pragma once

#include <memory>
#include <string>
#include "../schemas/agent_state.h"
#include "../schemas/orchestrator_context.h"
#include "intent_detector.h"
#include "planner.h"
#include "executor.h"
#include "evaluator.h"
#include "query_refiner.h"
#include "../context/context_builder.h"

namespace orchestrator {

class Agent {
public:
    Agent(std::shared_ptr<ToolRegistry> registry);

    OrchestratorContext process_query(
        const std::string& query, 
        const common::IdentityContext& identity, 
        const std::string& query_id);

private:
    std::shared_ptr<ToolRegistry> registry_;
    IntentDetector intent_detector_;
    Planner planner_;
    Evaluator evaluator_;
    QueryRefiner query_refiner_;
    ContextBuilder context_builder_;
};

} // namespace orchestrator
