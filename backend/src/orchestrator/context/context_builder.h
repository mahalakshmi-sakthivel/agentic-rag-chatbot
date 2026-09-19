#pragma once

#include "../schemas/agent_state.h"
#include "../schemas/orchestrator_context.h"

namespace orchestrator {

class ContextBuilder {
public:
    OrchestratorContext build(const AgentState& state) const;
};

} // namespace orchestrator
