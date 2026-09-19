#include "vector_search_tool.h"
#include "../../common/uuid.h"
#include "../../common/error_codes.h"
#include <chrono>

namespace orchestrator {

ToolResult VectorSearchTool::execute(
    const nlohmann::json& input,
    const common::IdentityContext& identity
) {
    auto start_time = std::chrono::steady_clock::now();
    
    ToolResult result;
    result.tool_name = name();
    result.input = input;

    if (!input.contains("query_text") || !input["query_text"].is_string()) {
        result.status = "failed";
        result.error = {
            {"code", common::ErrorCodes::VALIDATION_ERROR},
            {"message", "Missing or invalid 'query_text'"}
        };
        auto end_time = std::chrono::steady_clock::now();
        result.execution_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        return result;
    }

    std::string query = input["query_text"].get<std::string>();

    // Check tenant isolation requirement
    if (identity.tenant_id.empty()) {
        result.status = "failed";
        result.error = {
            {"code", common::ErrorCodes::UNAUTHENTICATED},
            {"message", "Missing tenant context for vector search"}
        };
        auto end_time = std::chrono::steady_clock::now();
        result.execution_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
        return result;
    }

    // Mock response compliant with Phase 4 contract
    nlohmann::json chunks = nlohmann::json::array();
    
    if (query.find("revenue") != std::string::npos || query.find("Q3") != std::string::npos) {
        chunks.push_back({
            {"chunk_id", common::generate_uuid()},
            {"document_id", "doc-001"},
            {"text", "Q3 revenue was $1.25M, a 25% increase from Q2."},
            {"score", 0.94},
            {"source_location", "page 5"},
            {"filename", "Q3_Report.pdf"}
        });
    } else {
        // generic match
        chunks.push_back({
            {"chunk_id", common::generate_uuid()},
            {"document_id", "doc-002"},
            {"text", "General information about " + query},
            {"score", 0.85},
            {"source_location", "page 1"},
            {"filename", "General_Doc.pdf"}
        });
    }

    result.status = "success";
    result.output = {
        {"results", chunks}
    };
    result.sources = chunks; // Promote for convenience

    auto end_time = std::chrono::steady_clock::now();
    result.execution_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
    
    return result;
}

} // namespace orchestrator
