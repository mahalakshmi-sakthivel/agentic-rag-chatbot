#include "DocumentRepository.h"

namespace ingestion::repository
{

bool InMemoryDocumentRepository::saveDocument(
    const models::Document &document)
{
    documents[document.documentId] = document;
    return true;
}

bool InMemoryDocumentRepository::saveChunks(
    const std::string &documentId,
    const std::vector<models::Chunk> &documentChunks)
{
    chunks[documentId] = documentChunks;
    return true;
}

std::vector<models::Chunk> InMemoryDocumentRepository::getChunks(
    const std::string &documentId,
    const std::string &tenantId)
{
    auto documentIt = documents.find(documentId);

    if (documentIt == documents.end())
    {
        return {};
    }

    if (documentIt->second.tenantId != tenantId)
    {
        return {};
    }

    auto chunkIt = chunks.find(documentId);

    if (chunkIt == chunks.end())
    {
        return {};
    }

    return chunkIt->second;
}

bool InMemoryDocumentRepository::documentExistsInTenant(
    const std::string &documentId,
    const std::string &tenantId) const
{
    auto it = documents.find(documentId);
    return it != documents.end() && it->second.tenantId == tenantId;
}

bool InMemoryDocumentRepository::canAccessDocument(
    const std::string &documentId,
    const std::string &tenantId,
    const std::string &userId,
    bool isAdmin) const
{
    auto it = documents.find(documentId);
    if (it == documents.end() || it->second.tenantId != tenantId)
        return false;

    return isAdmin || it->second.ownerUserId == userId;
}

bool InMemoryDocumentRepository::getDocument(
    const std::string &documentId,
    const std::string &tenantId,
    const std::string &userId,
    bool isAdmin,
    models::Document &document)
{
    auto it = documents.find(documentId);

    if (it == documents.end())
    {
        return false;
    }

    if (it->second.tenantId != tenantId)
    {
        return false;
    }

    if (!isAdmin && it->second.ownerUserId != userId)
    {
        return false;
    }

    document = it->second;
    return true;
}

std::vector<models::Document> InMemoryDocumentRepository::listDocuments(
    const std::string &tenantId,
    const std::string &userId,
    bool isAdmin)
{
    std::vector<models::Document> result;

    for (const auto &[documentId, document] : documents)
    {
        if (document.tenantId == tenantId &&
            (isAdmin || document.ownerUserId == userId))
        {
            result.push_back(document);
        }
    }

    return result;
}

bool InMemoryDocumentRepository::deleteDocument(
    const std::string &documentId,
    const std::string &tenantId,
    const std::string &userId,
    bool isAdmin)
{
    auto it = documents.find(documentId);

    if (it == documents.end())
    {
        return false;
    }

    if (it->second.tenantId != tenantId)
    {
        return false;
    }

    if (!isAdmin && it->second.ownerUserId != userId)
    {
        return false;
    }

    documents.erase(it);
    chunks.erase(documentId);

    return true;
}

} // namespace ingestion::repository
