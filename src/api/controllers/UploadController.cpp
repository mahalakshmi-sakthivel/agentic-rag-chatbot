#include "UploadController.h"
#include "response_helpers.h"
#include "../models/UploadModels.h"
#include "../../common/uuid.h"
#include "../../common/envelope.h"
#include "../../common/identity.h"
#include <drogon/MultiPart.h>
#include <algorithm>

namespace api
{

namespace
{

// PHASE 1 PLACEHOLDER — active only until Phase 3 assigns
// UploadController::ingestionHandler to a real implementation (see the seam
// doc comment in UploadController.h). Mirrors QueryController's
// stubQueryHandler: shape-correct response, zero real work. In particular
// this does NOT write fileBytes to disk anywhere — Phase 1 must not do
// Phase 3's storage job (Section 5), so there is deliberately no local
// fallback storage path here to fall out of sync with once Phase 3 lands.
IngestionResult defaultIngestionStub(const IngestionRequest & /*request*/)
{
    IngestionResult result;
    result.success = true;
    result.documentId = common::generateUuid();
    result.status = "queued";
    return result;
}

} // namespace

IngestionHandlerFn UploadController::ingestionHandler = defaultIngestionStub;

UploadController::UploadController(common::AppConfig config) : config_(std::move(config)) {}

void UploadController::handle(const drogon::HttpRequestPtr &req,
                               std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    // PHASE 2 INTEGRATION POINT: same mechanism as QueryController — see
    // common/identity.h.
    const auto identity = common::authenticate(req);
    if (!identity.authenticated)
    {
        callback(errorResponse(common::ErrorCode::UNAUTHENTICATED,
                                "Missing or invalid authentication token"));
        return;
    }

    drogon::MultiPartParser parser;
    if (parser.parse(req) != 0)
    {
        callback(errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                "Request must be valid multipart/form-data"));
        return;
    }

    const auto &files = parser.getFiles();
    if (files.empty())
    {
        callback(errorResponse(common::ErrorCode::VALIDATION_ERROR, "Field 'file' is required"));
        return;
    }

    // Checklist item 3 — "unsupported multiple-file requests, if multiple
    // files aren't allowed". See the ASSUMPTION note in UploadModels.h:
    // Phase 1 currently supports exactly one file per request.
    if (files.size() > 1)
    {
        callback(errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                "Only a single file is allowed per upload request",
                                {{"files_received", files.size()}}));
        return;
    }

    const auto &params = parser.getParameters();

    // Checklist item 3 — reject unexpected multipart fields instead of
    // silently ignoring them.
    for (const auto &param : params)
    {
        const auto &allowed = api::upload::allowedUploadParamFields();
        if (std::find(allowed.begin(), allowed.end(), param.first) == allowed.end())
        {
            callback(errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                    "Unexpected field: " + param.first,
                                    {{"field", param.first}}));
            return;
        }
    }

    auto fileTypeIt = params.find("file_type");
    if (fileTypeIt == params.end() || api::upload::trim(fileTypeIt->second).empty())
    {
        callback(errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                "Field 'file_type' is required"));
        return;
    }

    // Checklist item 4 — normalize before validating/using downstream,
    // don't just check-then-discard the trim.
    const std::string fileType = api::upload::trim(fileTypeIt->second);
    if (!api::upload::isAllowedFileType(fileType))
    {
        callback(errorResponse(common::ErrorCode::UNSUPPORTED_FILE_TYPE,
                                "Unsupported file_type: " + fileType,
                                {{"allowed", api::upload::allowedFileTypes()}}));
        return;
    }

    // Section 8 / 11: metadata is opaque to Phase 1, but if present it must
    // at least be valid JSON — malformed input never gets forwarded to
    // Phase 3. Defaults to a JSON null (not an empty object) when absent, so
    // Phase 3 can tell "no metadata sent" apart from "metadata: {}".
    nlohmann::json metadata = nullptr;
    auto metadataIt = params.find("metadata");
    if (metadataIt != params.end() && !api::upload::trim(metadataIt->second).empty())
    {
        metadata = nlohmann::json::parse(metadataIt->second, nullptr, false);
        if (metadata.is_discarded())
        {
            callback(errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                    "Field 'metadata' must be valid JSON"));
            return;
        }
    }

    const auto &file = files.front();
    const std::size_t sizeBytes = file.fileLength();

    // Checklist item 3 — "empty upload": a file field with zero bytes of
    // content is rejected rather than forwarded downstream.
    if (sizeBytes == 0)
    {
        callback(errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                "Uploaded file must not be empty"));
        return;
    }

    if (sizeBytes > config_.maxUploadSizeBytes)
    {
        callback(errorResponse(common::ErrorCode::FILE_TOO_LARGE,
                                "Uploaded file exceeds the maximum allowed size",
                                {{"max_bytes", config_.maxUploadSizeBytes}}));
        return;
    }

    const std::string safeName = api::upload::sanitizeFilename(file.getFileName());

    // ---- Forward to Phase 3 (Section 11.2 steps 1-2 already done above by
    // Phase 1; everything past this point — storage, parsing, chunking — is
    // Phase 3's responsibility via the ingestionHandler seam) ----
    IngestionRequest ingestionRequest;
    ingestionRequest.fileBytes = std::string(file.fileContent());
    ingestionRequest.filename = safeName;
    ingestionRequest.fileType = fileType;
    ingestionRequest.metadata = std::move(metadata);
    ingestionRequest.identity = identity;

    const IngestionResult result = ingestionHandler(ingestionRequest);

    if (!result.success)
    {
        const std::string code =
            result.errorCode.empty() ? common::ErrorCode::INTERNAL_ERROR : result.errorCode;
        const std::string message =
            result.errorMessage.empty() ? "Ingestion failed" : result.errorMessage;
        callback(errorResponse(code, message));
        return;
    }

    // Section 8.2 response `data` object. document_id/status come from
    // Phase 3; filename/size_bytes/uploaded_at are things Phase 1 already
    // validated itself and doesn't need Phase 3 to echo back.
    nlohmann::json data{
        {"document_id", result.documentId},
        {"status", result.status},
        {"filename", safeName},
        {"size_bytes", sizeBytes},
        {"uploaded_at", common::nowIso8601()},
    };

    callback(successResponse(data, drogon::k201Created));
}

} // namespace api
