#pragma once

#include <string>
#include "../schemas/agent_state.h"

namespace orchestrator {

class IntentDetector {
public:
    IntentType analyze(const std::string& query) const;
};

} // namespace orchestrator
