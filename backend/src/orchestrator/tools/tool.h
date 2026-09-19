#pragma once

#include <string>
#include <nlohmann/json.hpp>
#include "../../common/identity.h"
#include "../schemas/agent_state.h"

namespace orchestrator {

class Tool {
public:
    virtual std::string name() const = 0;

    virtual ToolResult execute(
        const nlohmann::json& input,
        const common::IdentityContext& identity
    ) = 0;

    virtual ~Tool() = default;
};

} // namespace orchestrator
