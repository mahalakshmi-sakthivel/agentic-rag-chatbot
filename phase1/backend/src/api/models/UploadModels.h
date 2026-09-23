// api/models/UploadModels.h
//
// Pure helpers for the /v1/data/upload endpoint (Section 7.3 + 11).
// Framework-free so filename sanitization and the file_type enum check are
// unit testable in isolation.
//
#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace api::upload
{

inline const std::vector<std::string> &allowedFileTypes()
{
    static const std::vector<std::string> types = {"pdf", "csv", "xlsx", "json"};
    return types;
}

inline bool isAllowedFileType(const std::string &fileType)
{
    const auto &allowed = allowedFileTypes();
    return std::find(allowed.begin(), allowed.end(), fileType) != allowed.end();
}

// Checklist item 4 (string trimming) — same trim semantics as
// api::query::trim, duplicated here (header-only, zero cross-include cost)
// so UploadController can normalize file_type/metadata the same way
// QueryController normalizes query/session_id/conversation_id.
inline std::string trim(const std::string &s)
{
    const auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

// Checklist item 3 — multipart fields Phase 1 recognizes. Anything else in
// the form (e.g. a client accidentally or maliciously adding an extra
// field) is rejected rather than silently ignored, per "reject unexpected
// multipart fields". ASSUMPTION flagged for Team Lead: single-file-only is
// enforced below (Section 7.3 doesn't explicitly say multi-file is
// disallowed, but nothing in the response shape — one document_id — supports
// it either). Confirm with the team; if multi-file uploads are wanted this
// gate and the "one file" check in UploadController both need revisiting.
inline const std::vector<std::string> &allowedUploadParamFields()
{
    static const std::vector<std::string> fields = {"file_type", "metadata"};
    return fields;
}

// Section 11 — basic input sanitization before file data touches disk:
// strips any path components and rejects path-traversal / unsafe characters.
// Returns a filename safe to join onto FILE_STORAGE_PATH.
inline std::string sanitizeFilename(const std::string &original)
{
    // Take only the last path segment — defeats "../../etc/passwd" style input
    // regardless of which separator the client used.
    std::string base = original;
    const auto lastSlash = base.find_last_of("/\\");
    if (lastSlash != std::string::npos)
    {
        base = base.substr(lastSlash + 1);
    }

    // Allow-list: letters, digits, dot, dash, underscore. Everything else
    // becomes '_'. This also neutralizes null bytes and control characters.
    std::string safe;
    safe.reserve(base.size());
    for (char c : base)
    {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_')
        {
            safe += c;
        }
        else
        {
            safe += '_';
        }
    }

    // Collapse leading dots so ".." / ".hidden" can't reappear post-sanitize.
    size_t nonDot = safe.find_first_not_of('.');
    if (nonDot == std::string::npos)
    {
        return "unnamed_file";
    }
    safe = safe.substr(nonDot);

    return safe.empty() ? "unnamed_file" : safe;
}

} // namespace api::upload
