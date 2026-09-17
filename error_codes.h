// common/error_codes.h
//
// The ONLY error codes Phase 1 may emit (PHASE_1_CONTRACT.md Section 9).
// Do not invent ad hoc codes — if a new one is genuinely needed, that is a
// contract amendment, not a local decision.
//
#pragma once

#include <string>

namespace common
{

namespace ErrorCode
{
constexpr const char *VALIDATION_ERROR = "VALIDATION_ERROR";
constexpr const char *UNSUPPORTED_FILE_TYPE = "UNSUPPORTED_FILE_TYPE";
constexpr const char *FILE_TOO_LARGE = "FILE_TOO_LARGE";
constexpr const char *NOT_FOUND = "NOT_FOUND";
constexpr const char *INTERNAL_ERROR = "INTERNAL_ERROR";
} // namespace ErrorCode

// Plain int so this header has zero framework dependency and stays unit
// testable without linking Drogon. Controllers cast this to
// drogon::HttpStatusCode at the response boundary.
inline int httpStatusForErrorCode(const std::string &code)
{
    if (code == ErrorCode::VALIDATION_ERROR) return 400;
    if (code == ErrorCode::UNSUPPORTED_FILE_TYPE) return 415;
    if (code == ErrorCode::FILE_TOO_LARGE) return 413;
    if (code == ErrorCode::NOT_FOUND) return 404;
    return 500; // INTERNAL_ERROR and any unmapped code
}

} // namespace common
