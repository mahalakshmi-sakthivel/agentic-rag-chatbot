#include "UploadController.h"
#include "response_helpers.h"
#include "../models/UploadModels.h"
#include "../../common/uuid.h"
#include "../../common/envelope.h"
#include "../../common/identity.h"
#include <drogon/MultiPart.h>
#include <filesystem>

namespace api
{

namespace fs = std::filesystem;

UploadController::UploadController(common::AppConfig config) : config_(std::move(config)) {}

void UploadController::handle(const drogon::HttpRequestPtr &req,
                               std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    // PHASE 2 INTEGRATION POINT: same call site as QueryController — see
    // common/identity.h. Not enforced yet in Phase 1; `identity` is kept
    // (not discarded) so it's already in scope for the 401/403 check and
    // for stamping tenant_id/user_id on stored files once Phase 2 lands.
    [[maybe_unused]] const auto identity = common::authenticate(req);

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

    const auto &params = parser.getParameters();

    auto fileTypeIt = params.find("file_type");
    if (fileTypeIt == params.end() || fileTypeIt->second.empty())
    {
        callback(errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                "Field 'file_type' is required"));
        return;
    }

    const std::string fileType = fileTypeIt->second;
    if (!api::upload::isAllowedFileType(fileType))
    {
        callback(errorResponse(common::ErrorCode::UNSUPPORTED_FILE_TYPE,
                                "Unsupported file_type: " + fileType,
                                {{"allowed", api::upload::allowedFileTypes()}}));
        return;
    }

    // Section 8 / 11: metadata is opaque to Phase 1, but if present it must
    // at least be valid JSON — malformed input never touches disk.
    auto metadataIt = params.find("metadata");
    if (metadataIt != params.end() && !metadataIt->second.empty())
    {
        const auto parsedMeta = nlohmann::json::parse(metadataIt->second, nullptr, false);
        if (parsedMeta.is_discarded())
        {
            callback(errorResponse(common::ErrorCode::VALIDATION_ERROR,
                                    "Field 'metadata' must be valid JSON"));
            return;
        }
    }

    const auto &file = files.front();
    const std::size_t sizeBytes = file.fileLength();

    if (sizeBytes > config_.maxUploadSizeBytes)
    {
        callback(errorResponse(common::ErrorCode::FILE_TOO_LARGE,
                                "Uploaded file exceeds the maximum allowed size",
                                {{"max_bytes", config_.maxUploadSizeBytes}}));
        return;
    }

    const std::string safeName = api::upload::sanitizeFilename(file.getFileName());
    const std::string documentId = common::generateUuid();

    std::error_code ec;
    fs::create_directories(config_.fileStoragePath, ec);
    if (ec)
    {
        callback(errorResponse(common::ErrorCode::INTERNAL_ERROR,
                                "Failed to prepare storage directory"));
        return;
    }

    // Store keyed by document_id so Phase 3's ingestion pipeline has a
    // collision-free lookup key; original (sanitized) filename is preserved
    // in the response for display purposes.
    const fs::path destPath = fs::path(config_.fileStoragePath) / (documentId + "_" + safeName);

    if (file.saveAs(destPath.string()) != 0)
    {
        callback(errorResponse(common::ErrorCode::INTERNAL_ERROR, "Failed to save uploaded file"));
        return;
    }

    nlohmann::json data{
        {"document_id", documentId},
        {"status", "queued"},
        {"filename", safeName},
        {"size_bytes", sizeBytes},
        {"uploaded_at", common::nowIso8601()},
    };

    callback(successResponse(data, drogon::k201Created));
}

} // namespace api
