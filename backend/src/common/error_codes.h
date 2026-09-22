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
// PARSE_FAILURE and EMPTY_DOCUMENT were added to complete the error set the
// contract already reserves for the Phase 1 -> Phase 3 handoff (Section
// 11.3 "Error conditions"). They are not new scope — Phase 1 never raises
// them itself; they only exist here so UploadController can pass a Phase 3
// ingestionHandler failure straight through the standard envelope
// (common/envelope.h) using a documented code instead of collapsing every
// ingestion failure into a generic INTERNAL_ERROR.
//
// PERMISSION_DENIED was added by Phase 2 for authorization (RBAC) denials —
// distinct from UNAUTHENTICATED (who are you?) vs PERMISSION_DENIED (you can't do that).
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
constexpr const char *PERMISSION_DENIED = "PERMISSION_DENIED";
constexpr const char *UNSUPPORTED_FILE_TYPE = "UNSUPPORTED_FILE_TYPE";
constexpr const char *FILE_TOO_LARGE = "FILE_TOO_LARGE";
constexpr const char *NOT_FOUND = "NOT_FOUND";
constexpr const char *INTERNAL_ERROR = "INTERNAL_ERROR";
// Section 11.3 — raised by a Phase 3 ingestionHandler (never by Phase 1
// itself) and surfaced verbatim through the same error envelope.
constexpr const char *PARSE_FAILURE = "PARSE_FAILURE";
constexpr const char *EMPTY_DOCUMENT = "EMPTY_DOCUMENT";
} // namespace ErrorCode

// Plain int so this header has zero framework dependency and stays unit
// testable without linking Drogon. Controllers cast this to
// drogon::HttpStatusCode at the response boundary.
inline int httpStatusForErrorCode(const std::string &code)
{
    if (code == ErrorCode::VALIDATION_ERROR) return 400;
    if (code == ErrorCode::UNAUTHENTICATED) return 401;
    if (code == ErrorCode::PERMISSION_DENIED) return 403;
    if (code == ErrorCode::UNSUPPORTED_FILE_TYPE) return 415;
    if (code == ErrorCode::FILE_TOO_LARGE) return 413;
    if (code == ErrorCode::NOT_FOUND) return 404;
    // 422: syntactically valid request, file content Phase 3 could not use.
    if (code == ErrorCode::PARSE_FAILURE) return 422;
    if (code == ErrorCode::EMPTY_DOCUMENT) return 422;
    return 500; // INTERNAL_ERROR and any unmapped code
}

} // namespace common
