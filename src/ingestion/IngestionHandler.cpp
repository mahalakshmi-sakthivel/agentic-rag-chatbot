#include "IngestionHandler.h"

#include "common/envelope.h"
#include "common/uuid.h"

#include "ingestion/chunking/TextChunker.h"
#include "ingestion/cleaning/TextCleaner.h"
#include "ingestion/metadata/ChunkIdGenerator.h"
#include "ingestion/metadata/MetadataExtractor.h"
#include "ingestion/models/Chunk.h"
#include "ingestion/models/Document.h"
#include "ingestion/parsers/ParserFactory.h"
#include "ingestion/repository/DocumentRepository.h"
#include "ingestion/repository/PersistentRawFileRepository.h"
#include "ingestion/validation/FileValidator.h"

namespace ingestion
{

repository::InMemoryDocumentRepository documentRepository;
repository::PersistentRawFileRepository rawRepository;

namespace
{

void markFailed(
    models::Document &document,
    api::IngestionResult &result,
    const std::string &errorCode,
    const std::string &errorMessage)
{
    document.status = "failed";
    document.errorCode = errorCode;
    documentRepository.saveDocument(document);

    result.success = false;
    result.status = "failed";
    result.errorCode = errorCode;
    result.errorMessage = errorMessage;
}

} // namespace

bool deleteRawFile(
    const std::string &documentId,
    const std::string &tenantId)
{
    return rawRepository.deleteRawFile(documentId, tenantId);
}

api::IngestionResult handleIngestion(
    const api::IngestionRequest &request)
{
    api::IngestionResult result;

    const std::string &tenantId = request.identity.tenant_id;
    const std::string &userId = request.identity.user_id;

    if (tenantId.empty() || userId.empty())
    {
        result.status = "failed";
        result.errorCode = "INTERNAL_ERROR";
        result.errorMessage = "Missing identity context";
        return result;
    }

    const std::string documentId = common::generateUuid();
    result.documentId = documentId;

    // Create and persist the logical document immediately in the QUEUED state.
    // This makes the full lifecycle observable through the repository.
    models::Document document;
    document.documentId = documentId;
    document.tenantId = tenantId;
    document.ownerUserId = userId;
    document.filename = request.filename;
    document.fileType = request.fileType;
    document.status = "queued";
    document.uploadedAt = common::nowIso8601();
    document.errorCode.clear();

    // Preserve the request metadata exactly:
    // request.metadata -> IngestionRequest.metadata -> Document.metadata -> repository.
    document.metadata = request.metadata;

    if (!documentRepository.saveDocument(document))
    {
        result.status = "failed";
        result.errorCode = "INTERNAL_ERROR";
        result.errorMessage = "Unable to save document";
        return result;
    }

    // Raw file storage happens before validation/processing, per Phase 3 contract.
    if (!rawRepository.saveRawFile(
            documentId,
            tenantId,
            request.filename,
            request.fileBytes))
    {
        markFailed(
            document,
            result,
            "INTERNAL_ERROR",
            "Unable to store uploaded file");
        return result;
    }

    if (request.fileBytes.empty())
    {
        markFailed(
            document,
            result,
            "EMPTY_DOCUMENT",
            "Uploaded file is empty");
        return result;
    }

    // Phase 1 normally performs request-level validation first. Keeping this
    // validation here also makes the Phase 3 entry point safe to call directly.
    const auto validation =
        validation::FileValidator::validate(
            request.filename,
            request.fileType,
            request.fileBytes);

    if (!validation.valid)
    {
        markFailed(
            document,
            result,
            validation.errorCode,
            validation.errorMessage);
        return result;
    }

    auto parser = parsers::ParserFactory::create(request.fileType);
    if (!parser)
    {
        markFailed(
            document,
            result,
            "UNSUPPORTED_FILE_TYPE",
            "Unsupported file type");
        return result;
    }

    // Persist PROCESSING before any parse/clean/chunk work.
    document.status = "processing";
    document.errorCode.clear();
    if (!documentRepository.saveDocument(document))
    {
        result.status = "failed";
        result.errorCode = "INTERNAL_ERROR";
        result.errorMessage = "Unable to persist processing status";
        return result;
    }

    const auto parsed = parser->parse(request.fileBytes);
    if (!parsed.success)
    {
        markFailed(
            document,
            result,
            parsed.errorCode,
            parsed.errorMessage);
        return result;
    }

    // Contracted chunking choice: 4000 characters with 800 characters overlap.
    // This is intentionally explicit so the Phase 3 implementation matches
    // the agreed contract and remains easy to verify.
    constexpr std::size_t chunkSize = 4000;
    constexpr std::size_t overlap = 800;

    std::vector<models::Chunk> chunks;

    const auto extractedMetadata =
        metadata::MetadataExtractor::extract(
            request.filename,
            request.fileType,
            tenantId,
            userId);

    int chunkIndex = 0;
    for (const auto &unit : parsed.units)
    {
        const std::string cleanedText =
            cleaning::TextCleaner::clean(unit.text);

        if (cleanedText.empty())
        {
            continue;
        }

        const auto chunkTexts =
            chunking::TextChunker::chunk(
                cleanedText,
                chunkSize,
                overlap);

        for (const auto &text : chunkTexts)
        {
            models::Chunk chunk;
            chunk.chunkId =
                metadata::ChunkIdGenerator::generate(
                    documentId,
                    chunkIndex);
            chunk.documentId = documentId;
            chunk.tenantId = tenantId;
            chunk.chunkIndex = chunkIndex;
            chunk.text = text;
            chunk.sourceLocation =
                unit.sourceLocation.empty()
                    ? extractedMetadata.sourceLocation
                    : unit.sourceLocation;

            // Required Phase 3 -> Phase 4 chunk metadata.
            chunk.metadata = {
                {"filename", request.filename},
                {"uploaded_by", userId}
            };

            chunks.push_back(std::move(chunk));
            ++chunkIndex;
        }
    }

    if (chunks.empty())
    {
        markFailed(
            document,
            result,
            "EMPTY_DOCUMENT",
            "Document produced no chunks");
        return result;
    }

    if (!documentRepository.saveChunks(documentId, chunks))
    {
        markFailed(
            document,
            result,
            "INTERNAL_ERROR",
            "Unable to save document chunks");
        return result;
    }

    // Processing completed successfully: persist READY and clear any failure.
    document.status = "ready";
    document.errorCode.clear();

    if (!documentRepository.saveDocument(document))
    {
        result.status = "failed";
        result.errorCode = "INTERNAL_ERROR";
        result.errorMessage = "Unable to persist ready status";
        return result;
    }

    result.success = true;
    result.documentId = documentId;
    result.status = "ready";

    return result;
}

} // namespace ingestion
