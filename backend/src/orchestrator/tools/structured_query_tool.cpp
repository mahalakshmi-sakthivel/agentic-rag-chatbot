#include "structured_query_tool.h"
#include "../../common/uuid.h"
#include "../../common/error_codes.h"
#include <chrono>

namespace orchestrator {

ToolResult StructuredQueryTool::execute(
    const nlohmann::json& input,
    const common::IdentityContext& identity
) {
    auto start_time = std::chrono::steady_clock::now();
    ToolResult result;
    result.tool_name = name();
    result.input = input;

    if (!input.contains("query") || !input["query"].is_string()) {
        result.status = "failed";
        result.error = {
            {"code", common::ErrorCodes::VALIDATION_ERROR},
            {"message", "Missing or invalid 'query'"}
        };
        auto end_time = std::chrono::steady_clock::now();
        result.execution_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        return result;
    }

    // Mock structured query
    result.status = "success";
    result.output = {
        {"columns", {"quarter", "revenue"}},
        {"rows", nlohmann::json::array({{{"quarter", "Q2"}, {"revenue", 1000000}}, {{"quarter", "Q3"}, {"revenue", 1250000}}})},
        {"source", {
            {"document_id", "doc-financials"},
            {"location", "Sheet1"}
        }}
    };
    
    // For context building, treat rows as chunks
    nlohmann::json chunks = nlohmann::json::array();
    chunks.push_back({
        {"chunk_id", common::generate_uuid()},
        {"document_id", "doc-financials"},
        {"text", "Structured Data: Q2 revenue=1000000, Q3 revenue=1250000"},
        {"score", 1.0},
        {"source_location", "Sheet1"},
        {"filename", "financials.xlsx"}
    });
    
    result.sources = chunks;

    auto end_time = std::chrono::steady_clock::now();
    result.execution_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return result;
}

} // namespace orchestrator
