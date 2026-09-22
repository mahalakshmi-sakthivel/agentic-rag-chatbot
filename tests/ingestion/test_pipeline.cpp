#include "ingestion/IngestionHandler.h"
#include "ingestion/handoff/Phase4ChunkHandoff.h"
#include "ingestion/models/Document.h"
#include "ingestion/repository/DocumentRepository.h"
#include "ingestion/metadata/ChunkIdGenerator.h"
#include <gtest/gtest.h>
#include <regex>
#include <filesystem>
#include <fstream>
#include "ingestion/repository/PersistentRawFileRepository.h"

namespace ingestion
{
extern repository::InMemoryDocumentRepository documentRepository;
}

using namespace ingestion;
using namespace ingestion::handoff;
using namespace api;

namespace
{
bool isIso8601Utc(const std::string &value)
{
    static const std::regex pattern(
        R"(^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}Z$)");
    return std::regex_match(value, pattern);
}
}

TEST(IngestionPipelineTest, PreservesSourceLocationsAndLifecycle)
{
    IngestionRequest req;
    req.filename = "test.csv";
    req.fileType = "csv";
    req.fileBytes = "col1,col2\nval1,val2\nval3,val4";
    req.identity.tenant_id = "tenant-1";
    req.identity.user_id = "user-1";

    auto result = handleIngestion(req);
    ASSERT_TRUE(result.success);

    auto chunks = documentRepository.getChunks(result.documentId, "tenant-1");
    ASSERT_GT(chunks.size(), 0);
    EXPECT_EQ(chunks[0].sourceLocation, "row 1");

    models::Document doc;
    ASSERT_TRUE(documentRepository.getDocument(
        result.documentId, "tenant-1", "user-1", false, doc));

    EXPECT_EQ(result.status, "ready");
    EXPECT_EQ(doc.status, "ready");
    EXPECT_TRUE(isIso8601Utc(doc.uploadedAt));
    EXPECT_TRUE(doc.errorCode.empty());
}

TEST(IngestionPipelineTest, PreservesDocumentMetadata)
{
    IngestionRequest req;
    req.filename = "metadata.csv";
    req.fileType = "csv";
    req.fileBytes = "col1\nvalue";
    req.metadata = {
        {"project", "weather"},
        {"classification", "internal"}
    };
    req.identity.tenant_id = "tenant-meta";
    req.identity.user_id = "user-meta";

    auto result = handleIngestion(req);
    ASSERT_TRUE(result.success);

    models::Document doc;
    ASSERT_TRUE(documentRepository.getDocument(
        result.documentId, "tenant-meta", "user-meta", false, doc));

    EXPECT_EQ(
        doc.metadata,
        req.metadata);
}

TEST(IngestionPipelineTest, PopulatesRequiredChunkMetadata)
{
    IngestionRequest req;
    req.filename = "metadata.csv";
    req.fileType = "csv";
    req.fileBytes = "col1\nvalue";
    req.identity.tenant_id = "tenant-chunk";
    req.identity.user_id = "user-chunk";

    auto result = handleIngestion(req);
    ASSERT_TRUE(result.success);

    auto chunks = documentRepository.getChunks(
        result.documentId, "tenant-chunk");
    ASSERT_GT(chunks.size(), 0);

    for (const auto &chunk : chunks)
    {
        EXPECT_EQ(chunk.metadata["filename"], "metadata.csv");
        EXPECT_EQ(chunk.metadata["uploaded_by"], "user-chunk");
    }
}

TEST(IngestionPipelineTest, Phase4HandoffUsesExactChunkSchema)
{
    IngestionRequest req;
    req.filename = "handoff.csv";
    req.fileType = "csv";
    req.fileBytes = "col1\nvalue";
    req.identity.tenant_id = "tenant-handoff";
    req.identity.user_id = "user-handoff";

    auto result = handleIngestion(req);
    ASSERT_TRUE(result.success);

    // Phase 4 depends only on the stable interface, not the concrete repository.
    IPhase4ChunkProvider &phase4Provider = documentRepository;
    const auto chunks = phase4Provider.getChunks(
        result.documentId, "tenant-handoff");

    ASSERT_GT(chunks.size(), 0);

    const auto &chunk = chunks.front();
    EXPECT_FALSE(chunk.chunkId.empty());
    EXPECT_EQ(chunk.documentId, result.documentId);
    EXPECT_EQ(chunk.tenantId, "tenant-handoff");
    EXPECT_EQ(chunk.chunkIndex, 0);
    EXPECT_FALSE(chunk.text.empty());
    EXPECT_FALSE(chunk.sourceLocation.empty());
    EXPECT_TRUE(chunk.metadata.is_object());
    EXPECT_EQ(chunk.metadata["filename"], "handoff.csv");
    EXPECT_EQ(chunk.metadata["uploaded_by"], "user-handoff");
}

TEST(IngestionPipelineTest, EmptyDocumentIsPersistedAsFailed)
{
    IngestionRequest req;
    req.filename = "empty.csv";
    req.fileType = "csv";
    req.fileBytes = "";
    req.identity.tenant_id = "tenant-fail";
    req.identity.user_id = "user-fail";

    auto result = handleIngestion(req);
    ASSERT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, "EMPTY_DOCUMENT");
    EXPECT_EQ(result.status, "failed");

    models::Document doc;
    ASSERT_TRUE(documentRepository.getDocument(
        result.documentId, "tenant-fail", "user-fail", false, doc));
    EXPECT_EQ(doc.status, "failed");
    EXPECT_EQ(doc.errorCode, "EMPTY_DOCUMENT");
}

TEST(IngestionPipelineTest, TenantIsolation_CannotAccessOtherTenantDocs)
{
    IngestionRequest req;
    req.filename = "test.csv";
    req.fileType = "csv";
    req.fileBytes = "col1\nval1";
    req.identity.tenant_id = "tenant-A";
    req.identity.user_id = "user-1";

    auto result = handleIngestion(req);
    ASSERT_TRUE(result.success);

    models::Document doc;
    EXPECT_FALSE(documentRepository.getDocument(
        result.documentId, "tenant-B", "user-1", false, doc));

    const auto chunks =
        documentRepository.getChunks(result.documentId, "tenant-B");
    EXPECT_TRUE(chunks.empty());
}


TEST(IngestionPipelineTest, ChunkIdsAreDeterministicUuidV5)
{
    const auto id1 = metadata::ChunkIdGenerator::generate("document-uuid", 0);
    const auto id2 = metadata::ChunkIdGenerator::generate("document-uuid", 0);
    const auto id3 = metadata::ChunkIdGenerator::generate("document-uuid", 1);

    EXPECT_EQ(id1, id2);
    EXPECT_NE(id1, id3);
    ASSERT_EQ(id1.size(), 36u);
    EXPECT_EQ(id1[14], '5');
    EXPECT_TRUE(
        id1[19] == '8' || id1[19] == '9' || id1[19] == 'a' || id1[19] == 'b');
}


TEST(IngestionPipelineTest, RawFileStoragePersistsToDisk)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "phase3_raw_storage_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    repository::PersistentRawFileRepository repo(root);
    ASSERT_TRUE(repo.saveRawFile(
        "doc-persist",
        "tenant-persist",
        "example.csv",
        "a,b\n1,2"));

    const auto path = root / "tenant-persist" / "doc-persist.raw";
    ASSERT_TRUE(std::filesystem::exists(path));

    std::ifstream input(path, std::ios::binary);
    std::string bytes(
        (std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>());
    EXPECT_EQ(bytes, "a,b\n1,2");

    std::filesystem::remove_all(root, ec);
}


TEST(IngestionPipelineTest, UserAuthorizationAllowsOwnerAndAdminOnly)
{
    models::Document ownerDoc;
    ownerDoc.documentId = "auth-doc-owner";
    ownerDoc.tenantId = "auth-tenant";
    ownerDoc.ownerUserId = "owner-user";
    ownerDoc.status = "ready";
    documentRepository.saveDocument(ownerDoc);

    models::Document loaded;
    EXPECT_TRUE(documentRepository.getDocument(
        ownerDoc.documentId, "auth-tenant", "owner-user", false, loaded));
    EXPECT_FALSE(documentRepository.getDocument(
        ownerDoc.documentId, "auth-tenant", "other-user", false, loaded));
    EXPECT_TRUE(documentRepository.getDocument(
        ownerDoc.documentId, "auth-tenant", "admin-user", true, loaded));

    EXPECT_TRUE(documentRepository.canAccessDocument(
        ownerDoc.documentId, "auth-tenant", "owner-user", false));
    EXPECT_FALSE(documentRepository.canAccessDocument(
        ownerDoc.documentId, "auth-tenant", "other-user", false));
    EXPECT_TRUE(documentRepository.canAccessDocument(
        ownerDoc.documentId, "auth-tenant", "admin-user", true));
}


TEST(IngestionPipelineTest, RawFileStorageDeletesOnRequest)
{
    const std::filesystem::path root =
        std::filesystem::temp_directory_path() / "phase3_raw_storage_delete_test";
    std::error_code ec;
    std::filesystem::remove_all(root, ec);

    repository::PersistentRawFileRepository repo(root);
    ASSERT_TRUE(repo.saveRawFile(
        "doc-delete",
        "tenant-delete",
        "example.csv",
        "a,b\n1,2"));

    const auto path = root / "tenant-delete" / "doc-delete.raw";
    ASSERT_TRUE(std::filesystem::exists(path));
    EXPECT_TRUE(repo.deleteRawFile("doc-delete", "tenant-delete"));
    EXPECT_FALSE(std::filesystem::exists(path));
    EXPECT_TRUE(repo.deleteRawFile("doc-delete", "tenant-delete"));

    std::filesystem::remove_all(root, ec);
}
