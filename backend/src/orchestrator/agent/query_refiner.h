#pragma once

#include <string>
#include "../schemas/agent_state.h"

namespace orchestrator {

class QueryRefiner {
public:
    bool refine(AgentState& state) const;
};

} // namespace orchestrator
