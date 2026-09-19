#pragma once

#include "../schemas/agent_state.h"

namespace orchestrator {

class Evaluator {
public:
    bool is_sufficient(const AgentState& state) const;
};

} // namespace orchestrator
