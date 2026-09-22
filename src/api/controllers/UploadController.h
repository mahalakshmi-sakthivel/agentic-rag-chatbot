// api/controllers/UploadController.h
//
// Section 7.3 / 8.2 — POST /v1/data/upload.
//
// Phase 1's job here is exactly: receive the multipart request, validate it
// (auth, single file, allowed file_type, size limit, well-formed metadata,
// no unexpected fields, non-empty file, sanitized filename), then FORWARD
// the validated file + metadata + IdentityContext to Phase 3 for storage
// and processing. Per Section 5 ("Must NOT Do: ... direct DB access
// bypassing modules") and Section 11 ("Owned by Phase 3"), Phase 1 itself
// must not write the file to disk — that is Phase 3's responsibility.
//
// SEAM FOR PHASE 3 (Ingestion Pipeline): swap `UploadController::
// ingestionHandler` for the real ingestion entry point, exactly like
// `QueryController::queryHandler` is the seam for Phase 5/6. Phase 3's
// implementation lives in its own sibling module (src/ingestion/, per
// CMakeLists.txt's sibling-module discovery) and is wired in with one line
// in main.cpp — no controller code changes required. Until that line is
// added, `defaultIngestionStub` (UploadController.cpp) is the active
// handler: it returns a shape-correct placeholder (document_id + status
// "queued") and performs NO real storage, the same way QueryController's
// stub performs no real retrieval/LLM call.
//
#pragma once

#include "../../common/config.h"
#include "../../common/identity.h"
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <functional>
#include <nlohmann/json.hpp>
#include <string>

namespace api
{

// Interface: Phase 1 -> Phase 3 (contract: TECHNICAL_CONTRACT.md Section
// 11.2 step 1-2 + Section 11.3). Everything in here has already passed
// Phase 1's validation (Section 8.3) — Phase 3 can trust file_type is one
// of the allowed enum values, filename is sanitized, and fileBytes is
// non-empty and within the configured size limit.
struct IngestionRequest
{
    std::string fileBytes;   // raw file content, already size/emptiness-checked
    std::string filename;    // sanitized (see api::upload::sanitizeFilename)
    std::string fileType;    // one of: pdf, csv, xlsx, json
    nlohmann::json metadata; // parsed JSON object from the 'metadata' field, or null if absent
    common::IdentityContext identity; // Section 9.2 shape; ingestion is always tenant/user-scoped
};

// Interface: Phase 3 -> Phase 1. On success, Phase 1 combines documentId +
// status with the filename/size_bytes/uploaded_at it already validated to
// build the Section 8.2 response body — Phase 3 does not need to echo those
// back. On failure, errorCode MUST be one of common::ErrorCode's Section
// 11.3 values (UNSUPPORTED_FILE_TYPE, FILE_TOO_LARGE, PARSE_FAILURE,
// EMPTY_DOCUMENT) or INTERNAL_ERROR for anything else.
struct IngestionResult
{
    bool success = false;

    // populated when success == true
    std::string documentId;
    std::string status = "queued"; // Section 11.4: queued|processing|ready|failed

    // populated when success == false
    std::string errorCode;
    std::string errorMessage;
};

using IngestionHandlerFn = std::function<IngestionResult(const IngestionRequest &)>;

class UploadController
{
public:
    explicit UploadController(common::AppConfig config);

    void handle(const drogon::HttpRequestPtr &req,
                std::function<void(const drogon::HttpResponsePtr &)> &&callback);

    // Defaults to Phase 1's placeholder (defaultIngestionStub in
    // UploadController.cpp). Phase 3 reassigns this — typically once, in
    // main.cpp at startup, e.g.:
    //
    //   #include "ingestion/IngestionHandler.h"   // Phase 3's header
    //   UploadController::ingestionHandler = ingestion::handleIngestion;
    //
    // No controller code changes required.
    static IngestionHandlerFn ingestionHandler;

private:
    common::AppConfig config_;
};

} // namespace api
