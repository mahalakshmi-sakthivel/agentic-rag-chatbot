#pragma once

#include "../schemas/agent_state.h"
#include "../tools/tool_registry.h"
#include "limits.h"

namespace orchestrator {

class Executor {
public:
    Executor(std::shared_ptr<ToolRegistry> registry, AgentLimits limits = AgentLimits());

    bool execute_step(AgentState& state);

private:
    std::shared_ptr<ToolRegistry> registry_;
    AgentLimits limits_;
};

} // namespace orchestrator
