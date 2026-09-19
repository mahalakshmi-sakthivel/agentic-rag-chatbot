// tests/api/test_query_validation.cpp
//
// Section 14: request validation tests (missing field, unknown field, empty
// string, malformed JSON is covered at the parse layer in the controller —
// here we test the parsed-JSON validation contract directly).
//
#include "api/models/QueryModels.h"
#include <gtest/gtest.h>

using api::query::validateQueryRequest;

TEST(QueryValidation, AcceptsMinimalValidRequest)
{
    nlohmann::json body = {
        {"query", "What was Q3 revenue?"},
        {"session_id", "123e4567-e89b-42d3-a456-426614174000"}};

    const auto result = validateQueryRequest(body);
    EXPECT_TRUE(result.valid);
}

TEST(QueryValidation, AcceptsFullyPopulatedRequest)
{
    nlohmann::json body = {
        {"query", "Compare Q2 and Q3"},
        {"session_id", "123e4567-e89b-42d3-a456-426614174000"},
        {"document_ids", {"doc-1", "doc-2"}},
        {"conversation_id", "223e4567-e89b-42d3-a456-426614174000"},
        {"options", {{"stream", false}}}};

    const auto result = validateQueryRequest(body);
    EXPECT_TRUE(result.valid);
}

TEST(QueryValidation, RejectsMissingQuery)
{
    nlohmann::json body = {{"session_id", "123e4567-e89b-42d3-a456-426614174000"}};
    const auto result = validateQueryRequest(body);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.errorDetails.value("field", ""), "query");
}

TEST(QueryValidation, RejectsEmptyQueryAfterTrim)
{
    nlohmann::json body = {
        {"query", "   \t  "},
        {"session_id", "123e4567-e89b-42d3-a456-426614174000"}};
    const auto result = validateQueryRequest(body);
    EXPECT_FALSE(result.valid);
}

TEST(QueryValidation, RejectsMissingSessionId)
{
    nlohmann::json body = {{"query", "hello"}};
    const auto result = validateQueryRequest(body);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.errorDetails.value("field", ""), "session_id");
}

TEST(QueryValidation, RejectsMalformedSessionId)
{
    nlohmann::json body = {{"query", "hello"}, {"session_id", "not-a-uuid"}};
    const auto result = validateQueryRequest(body);
    EXPECT_FALSE(result.valid);
}

TEST(QueryValidation, RejectsUnknownTopLevelField)
{
    nlohmann::json body = {
        {"query", "hello"},
        {"session_id", "123e4567-e89b-42d3-a456-426614174000"},
        {"debug_mode", true}};
    const auto result = validateQueryRequest(body);
    EXPECT_FALSE(result.valid);
    EXPECT_EQ(result.errorDetails.value("field", ""), "debug_mode");
}

TEST(QueryValidation, RejectsNonArrayDocumentIds)
{
    nlohmann::json body = {
        {"query", "hello"},
        {"session_id", "123e4567-e89b-42d3-a456-426614174000"},
        {"document_ids", "doc-1"}};
    const auto result = validateQueryRequest(body);
    EXPECT_FALSE(result.valid);
}

TEST(QueryValidation, RejectsNonObjectBody)
{
    nlohmann::json body = nlohmann::json::array({"not", "an", "object"});
    const auto result = validateQueryRequest(body);
    EXPECT_FALSE(result.valid);
}
