#pragma once

#include "tool.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace orchestrator {

class ToolRegistry {
public:
    void register_tool(std::shared_ptr<Tool> tool);
    std::shared_ptr<Tool> get_tool(const std::string& name) const;
    std::vector<std::string> get_available_tools() const;
    bool has_tool(const std::string& name) const;

private:
    std::unordered_map<std::string, std::shared_ptr<Tool>> tools_;
};

} // namespace orchestrator
