#pragma once

#include "tool.h"

namespace orchestrator {

class VectorSearchTool : public Tool {
public:
    std::string name() const override { return "vector_search"; }

    ToolResult execute(
        const nlohmann::json& input,
        const common::IdentityContext& identity
    ) override;
};

} // namespace orchestrator
