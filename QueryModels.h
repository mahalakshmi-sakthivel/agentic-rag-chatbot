// api/models/QueryModels.h
//
// Pure validation logic for the /v1/query request body (Section 7.2 + 8).
// Deliberately framework-free (no Drogon include) so it can be unit tested
// directly — see tests/api/test_validation.cpp.
//
#pragma once

#include <nlohmann/json.hpp>
#include <algorithm>
#include <regex>
#include <string>
#include <vector>

namespace api::query
{

struct ValidationResult
{
    bool valid = false;
    std::string errorMessage;
    nlohmann::json errorDetails = nlohmann::json::object();
};

inline std::string trim(const std::string &s)
{
    const auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    const auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

inline bool isValidUuidLike(const std::string &s)
{
    // Deliberately permissive (any UUID version), matches Section 7.2's
    // "string (UUID)" requirement without over-constraining Phase 2/5/6.
    static const std::regex uuidRegex(
        "^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-"
        "[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$");
    return std::regex_match(s, uuidRegex);
}

// Section 8: reject unknown top-level fields (strict schema).
inline const std::vector<std::string> &allowedQueryFields()
{
    static const std::vector<std::string> fields = {
        "query", "session_id", "document_ids", "conversation_id", "options"};
    return fields;
}

// Returns a ValidationResult. On success, `valid` is true and the caller can
// trust the shape of `body`. On failure, errorMessage/errorDetails are ready
// to hand straight to common::makeErrorEnvelope with ErrorCode::VALIDATION_ERROR.
inline ValidationResult validateQueryRequest(const nlohmann::json &body)
{
    ValidationResult result;

    if (!body.is_object())
    {
        result.errorMessage = "Request body must be a JSON object";
        return result;
    }

    // Strict schema — unknown top-level fields are rejected.
    for (auto it = body.begin(); it != body.end(); ++it)
    {
        const auto &key = it.key();
        const auto &allowed = allowedQueryFields();
        if (std::find(allowed.begin(), allowed.end(), key) == allowed.end())
        {
            result.errorMessage = "Unknown field: " + key;
            result.errorDetails = {{"field", key}};
            return result;
        }
    }

    // query — required, non-empty after trim
    if (!body.contains("query") || !body.at("query").is_string())
    {
        result.errorMessage = "'query' is required and must be a string";
        result.errorDetails = {{"field", "query"}};
        return result;
    }
    if (trim(body.at("query").get<std::string>()).empty())
    {
        result.errorMessage = "'query' must not be empty";
        result.errorDetails = {{"field", "query"}};
        return result;
    }

    // session_id — required, UUID string
    if (!body.contains("session_id") || !body.at("session_id").is_string())
    {
        result.errorMessage = "'session_id' is required and must be a UUID string";
        result.errorDetails = {{"field", "session_id"}};
        return result;
    }
    if (!isValidUuidLike(body.at("session_id").get<std::string>()))
    {
        result.errorMessage = "'session_id' must be a valid UUID";
        result.errorDetails = {{"field", "session_id"}};
        return result;
    }

    // document_ids — optional, array of strings
    if (body.contains("document_ids"))
    {
        if (!body.at("document_ids").is_array())
        {
            result.errorMessage = "'document_ids' must be an array of strings";
            result.errorDetails = {{"field", "document_ids"}};
            return result;
        }
        for (const auto &item : body.at("document_ids"))
        {
            if (!item.is_string())
            {
                result.errorMessage = "'document_ids' must contain only strings";
                result.errorDetails = {{"field", "document_ids"}};
                return result;
            }
        }
    }

    // conversation_id — optional, UUID string
    if (body.contains("conversation_id"))
    {
        if (!body.at("conversation_id").is_string() ||
            !isValidUuidLike(body.at("conversation_id").get<std::string>()))
        {
            result.errorMessage = "'conversation_id' must be a valid UUID";
            result.errorDetails = {{"field", "conversation_id"}};
            return result;
        }
    }

    // options — optional, object
    if (body.contains("options") && !body.at("options").is_object())
    {
        result.errorMessage = "'options' must be an object";
        result.errorDetails = {{"field", "options"}};
        return result;
    }

    result.valid = true;
    return result;
}

} // namespace api::query
