#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace ingestion::models
{

// Chunk Output Schema — Phase 3 → Phase 4 interface.
// Field names are FROZEN by TECHNICAL_CONTRACT.md Section 11.3 and §26.
// Do NOT rename fields without a Section 27 amendment.
struct Chunk
{
    std::string chunkId;       // UUID v5 (deterministic from a fixed namespace and documentId+index)
    std::string documentId;    // UUID — back-reference to parent document
    std::string tenantId;      // UUID — mandatory isolation stamp (§10.2)

    int chunkIndex = 0;        // 0-based position in the document

    std::string text;          // cleaned chunk text — must not be empty (§11.3)
    std::string sourceLocation; // e.g. "page 3" | "row 12" | "$.orders[4]" | "sheet 'S1' row 5"

    // Optional §11.3 metadata object: { "filename": "...", "uploaded_by": "uuid" }
    nlohmann::json metadata = nlohmann::json::object();
};

} // namespace ingestion::models
