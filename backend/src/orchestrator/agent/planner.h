#pragma once

#include <vector>
#include "../schemas/agent_state.h"

namespace orchestrator {

class Planner {
public:
    std::vector<StepPlan> create_plan(const AgentState& state) const;
};

} // namespace orchestrator
