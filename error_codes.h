// common/error_codes.h
//
// The error codes Phase 1 may emit. Originally scoped to 5 codes per
// PHASE_1_CONTRACT.md Section 9 (UNAUTHENTICATED was explicitly "not yours
// to implement"). UNAUTHENTICATED was added here on Team Lead's explicit
// review instruction to connect real enforcement on /v1/query and
// /v1/data/upload before Phase 2's JWT verifier exists — see
// common/identity.h for the mechanism. This is a recorded scope extension,
// not a unilateral addition: flag it in the PR description referencing this
// review round so it's on the record per Section 27.
//
// Do not add further ad hoc codes beyond this — a genuinely new one still
// needs a contract amendment.
//
#pragma once

#include <string>

namespace common
{

namespace ErrorCode
{
constexpr const char *VALIDATION_ERROR = "VALIDATION_ERROR";
constexpr const char *UNAUTHENTICATED = "UNAUTHENTICATED";
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
    if (code == ErrorCode::UNAUTHENTICATED) return 401;
    if (code == ErrorCode::UNSUPPORTED_FILE_TYPE) return 415;
    if (code == ErrorCode::FILE_TOO_LARGE) return 413;
    if (code == ErrorCode::NOT_FOUND) return 404;
    return 500; // INTERNAL_ERROR and any unmapped code
}

} // namespace common
