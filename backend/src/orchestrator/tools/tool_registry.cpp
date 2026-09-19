#include "tool_registry.h"

namespace orchestrator {

void ToolRegistry::register_tool(std::shared_ptr<Tool> tool) {
    if (tool) {
        tools_[tool->name()] = tool;
    }
}

std::shared_ptr<Tool> ToolRegistry::get_tool(const std::string& name) const {
    auto it = tools_.find(name);
    if (it != tools_.end()) {
        return it->second;
    }
    return nullptr;
}

std::vector<std::string> ToolRegistry::get_available_tools() const {
    std::vector<std::string> names;
    names.reserve(tools_.size());
    for (const auto& pair : tools_) {
        names.push_back(pair.first);
    }
    return names;
}

bool ToolRegistry::has_tool(const std::string& name) const {
    return tools_.find(name) != tools_.end();
}

} // namespace orchestrator
