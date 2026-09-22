#include "DataController.h"
#include "response_helpers.h"
#include "../../ingestion/models/Document.h"
#include "../../ingestion/repository/DocumentRepository.h"
#include "../../ingestion/IngestionHandler.h"
#include <algorithm>

namespace ingestion
{
extern repository::InMemoryDocumentRepository documentRepository;
}

namespace api
{

namespace
{
bool hasAdminRole(const common::IdentityContext &identity)
{
    return std::find(identity.roles.begin(), identity.roles.end(), "admin") != identity.roles.end();
}

} // namespace

void DataController::handleStatus(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &documentId)
{
    const auto identity = common::authenticate(req);

    if (!identity.authenticated)
    {
        callback(errorResponse(
            common::ErrorCode::UNAUTHENTICATED,
            "Missing or invalid authentication token"));
        return;
    }

    ingestion::models::Document document;

    if (!ingestion::documentRepository.getDocument(
        documentId,
        identity.tenant_id,
        identity.user_id,
        hasAdminRole(identity),
        document))
    {
        const auto code = ingestion::documentRepository.documentExistsInTenant(
            documentId, identity.tenant_id)
            ? common::ErrorCode::PERMISSION_DENIED
            : common::ErrorCode::NOT_FOUND;
        callback(errorResponse(
            code,
            code == common::ErrorCode::PERMISSION_DENIED
                ? "You do not have permission to access this document"
                : "Document not found"));
        return;
    }

    callback(successResponse({
        {"document_id", document.documentId},
        {"status", document.status},
        {"error_code", document.errorCode}
    }));
}

void DataController::handleList(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback)
{
    const auto identity = common::authenticate(req);

    if (!identity.authenticated)
    {
        callback(errorResponse(
            common::ErrorCode::UNAUTHENTICATED,
            "Missing or invalid authentication token"));
        return;
    }

    const auto documents =
    ingestion::documentRepository.listDocuments(
        identity.tenant_id,
        identity.user_id,
        hasAdminRole(identity));

    nlohmann::json result = nlohmann::json::array();

    for (const auto &document : documents)
    {
        nlohmann::json item = {
    {"document_id", document.documentId},
    {"filename", document.filename},
    {"file_type", document.fileType},
    {"status", document.status},
    {"uploaded_at", document.uploadedAt}
};

result.push_back(item);
    }

    callback(successResponse({
        {"documents", result}
    }));
}

void DataController::handleDelete(
    const drogon::HttpRequestPtr &req,
    std::function<void(const drogon::HttpResponsePtr &)> &&callback,
    const std::string &documentId)
{
    const auto identity = common::authenticate(req);

    if (!identity.authenticated)
    {
        callback(errorResponse(
            common::ErrorCode::UNAUTHENTICATED,
            "Missing or invalid authentication token"));
        return;
    }

    if (!ingestion::documentRepository.canAccessDocument(
        documentId,
        identity.tenant_id,
        identity.user_id,
        hasAdminRole(identity)))
    {
        const auto code = ingestion::documentRepository.documentExistsInTenant(
            documentId, identity.tenant_id)
            ? common::ErrorCode::PERMISSION_DENIED
            : common::ErrorCode::NOT_FOUND;
        callback(errorResponse(
            code,
            code == common::ErrorCode::PERMISSION_DENIED
                ? "You do not have permission to delete this document"
                : "Document not found"));
        return;
    }

    if (!ingestion::deleteRawFile(documentId, identity.tenant_id))
    {
        callback(errorResponse(
            common::ErrorCode::INTERNAL_ERROR,
            "Unable to delete raw file"));
        return;
    }

    if (!ingestion::documentRepository.deleteDocument(
        documentId,
        identity.tenant_id,
        identity.user_id,
        hasAdminRole(identity)))
    {
        callback(errorResponse(
            common::ErrorCode::INTERNAL_ERROR,
            "Unable to delete document"));
        return;
    }

    callback(successResponse({
        {"document_id", documentId},
        {"deleted", true}
    }));
}

} // namespace api
