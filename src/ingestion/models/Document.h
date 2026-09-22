#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace ingestion::models
{

// Logical Document schema per TECHNICAL_CONTRACT.md Section 12.1.
// All fields are required unless noted. Physical storage engine is TBD
// (Section 12.2) — shape is fixed here.
struct Document
{
    std::string documentId;    // UUID v4
    std::string tenantId;      // UUID — primary isolation boundary (§10)
    std::string ownerUserId;   // UUID — owner of this document

    std::string filename;      // sanitized filename
    std::string fileType;      // one of: pdf, csv, xlsx, json
    std::string status;        // queued | processing | ready | failed (§11.4)

    std::string uploadedAt;    // ISO 8601 UTC (§7.1)
    std::string errorCode;     // populated only when status == "failed"

    nlohmann::json metadata;   // optional arbitrary key-value tags from upload request
};

} // namespace ingestion::models
