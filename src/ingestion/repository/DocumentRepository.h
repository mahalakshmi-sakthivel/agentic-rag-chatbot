#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ingestion/models/Chunk.h"
#include "ingestion/models/Document.h"
#include "ingestion/handoff/Phase4ChunkHandoff.h"

namespace ingestion::repository
{

class DocumentRepository : public handoff::IPhase4ChunkProvider
{
public:
    virtual ~DocumentRepository() = default;

    virtual bool saveDocument(
        const models::Document &document) = 0;

    virtual bool saveChunks(
        const std::string &documentId,
        const std::vector<models::Chunk> &chunks) = 0;

    virtual bool documentExistsInTenant(
        const std::string &documentId,
        const std::string &tenantId) const = 0;

    virtual bool canAccessDocument(
        const std::string &documentId,
        const std::string &tenantId,
        const std::string &userId,
        bool isAdmin) const = 0;

    virtual std::vector<models::Chunk> getChunks(
        const std::string &documentId,
        const std::string &tenantId) override = 0;

    virtual bool getDocument(
        const std::string &documentId,
        const std::string &tenantId,
        const std::string &userId,
        bool isAdmin,
        models::Document &document) = 0;

    virtual std::vector<models::Document> listDocuments(
        const std::string &tenantId,
        const std::string &userId,
        bool isAdmin) = 0;

    virtual bool deleteDocument(
        const std::string &documentId,
        const std::string &tenantId,
        const std::string &userId,
        bool isAdmin) = 0;
};

class InMemoryDocumentRepository : public DocumentRepository
{
public:
    bool saveDocument(
        const models::Document &document) override;

    bool saveChunks(
        const std::string &documentId,
        const std::vector<models::Chunk> &chunks) override;

    bool documentExistsInTenant(
        const std::string &documentId,
        const std::string &tenantId) const override;

    bool canAccessDocument(
        const std::string &documentId,
        const std::string &tenantId,
        const std::string &userId,
        bool isAdmin) const override;

    std::vector<models::Chunk> getChunks(
        const std::string &documentId,
        const std::string &tenantId) override;

bool getDocument(
    const std::string &documentId,
    const std::string &tenantId,
    const std::string &userId,
    bool isAdmin,
    models::Document &document) override;

std::vector<models::Document> listDocuments(
    const std::string &tenantId,
    const std::string &userId,
    bool isAdmin) override;

bool deleteDocument(
    const std::string &documentId,
    const std::string &tenantId,
    const std::string &userId,
    bool isAdmin) override;

private:
    std::unordered_map<std::string, models::Document> documents;
    std::unordered_map<
        std::string,
        std::vector<models::Chunk>> chunks;
};

} // namespace ingestion::repository
