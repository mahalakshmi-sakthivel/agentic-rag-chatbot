#pragma once

#include "tool.h"

namespace orchestrator {

class CalculatorTool : public Tool {
public:
    std::string name() const override { return "calculator"; }

    ToolResult execute(
        const nlohmann::json& input,
        const common::IdentityContext& identity
    ) override;
};

} // namespace orchestrator
