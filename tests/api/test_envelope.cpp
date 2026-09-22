// tests/api/test_envelope.cpp
//
// Section 6/9: every response must use the standard envelope shape, and
// error envelopes must carry the documented error codes.
//
#include "common/envelope.h"
#include "common/error_codes.h"
#include <gtest/gtest.h>
#include <regex>

TEST(Envelope, SuccessEnvelopeHasRequiredTopLevelKeys)
{
    auto env = common::makeSuccessEnvelope({{"status", "ok"}}, "req-123");
    EXPECT_TRUE(env["success"].get<bool>());
    EXPECT_EQ(env["data"]["status"], "ok");
    EXPECT_EQ(env["meta"]["request_id"], "req-123");
    EXPECT_TRUE(env["meta"].contains("timestamp"));
    // Success envelope must not accidentally carry an "error" key.
    EXPECT_FALSE(env.contains("error"));
}

TEST(Envelope, ErrorEnvelopeHasRequiredShape)
{
    auto env = common::makeErrorEnvelope(
        common::ErrorCode::VALIDATION_ERROR, "bad input", {{"field", "query"}}, "req-456");

    EXPECT_FALSE(env["success"].get<bool>());
    EXPECT_EQ(env["error"]["code"], "VALIDATION_ERROR");
    EXPECT_EQ(env["error"]["message"], "bad input");
    EXPECT_EQ(env["error"]["details"]["field"], "query");
    EXPECT_EQ(env["meta"]["request_id"], "req-456");
    EXPECT_FALSE(env.contains("data"));
}

TEST(Envelope, TimestampIsIso8601Utc)
{
    const std::string ts = common::nowIso8601();
    static const std::regex iso8601(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)");
    EXPECT_TRUE(std::regex_match(ts, iso8601)) << "got: " << ts;
}

TEST(ErrorCodes, MapToDocumentedHttpStatuses)
{
    using namespace common;
    EXPECT_EQ(httpStatusForErrorCode(ErrorCode::VALIDATION_ERROR), 400);
    EXPECT_EQ(httpStatusForErrorCode(ErrorCode::UNSUPPORTED_FILE_TYPE), 415);
    EXPECT_EQ(httpStatusForErrorCode(ErrorCode::FILE_TOO_LARGE), 413);
    EXPECT_EQ(httpStatusForErrorCode(ErrorCode::NOT_FOUND), 404);
    EXPECT_EQ(httpStatusForErrorCode(ErrorCode::INTERNAL_ERROR), 500);
}
