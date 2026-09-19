#pragma once

#include "tool.h"

namespace orchestrator {

class StructuredQueryTool : public Tool {
public:
    std::string name() const override { return "structured_query"; }

    ToolResult execute(
        const nlohmann::json& input,
        const common::IdentityContext& identity
    ) override;
};

} // namespace orchestrator
