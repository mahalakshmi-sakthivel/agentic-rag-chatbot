#include "controllers/DataController.h"
#include "common/identity.h"
#include "ingestion/repository/DocumentRepository.h"
#include "ingestion/models/Document.h"

#include <drogon/HttpRequest.h>
#include <gtest/gtest.h>

namespace ingestion
{
extern repository::InMemoryDocumentRepository documentRepository;
}

TEST(DataAuthorizationTest, SameTenantDifferentUserGets403)
{
    ingestion::models::Document document;
    document.documentId = "auth-api-doc";
    document.tenantId = "tenant-api";
    document.ownerUserId = "owner-api";
    document.filename = "a.csv";
    document.fileType = "csv";
    document.status = "ready";
    ingestion::documentRepository.saveDocument(document);

    common::setTokenVerifier([](const drogon::HttpRequestPtr &) {
        common::IdentityContext identity;
        identity.authenticated = true;
        identity.user_id = "other-api-user";
        identity.tenant_id = "tenant-api";
        identity.roles = {"user"};
        identity.session_id = "session-api";
        return identity;
    });

    auto req = drogon::HttpRequest::newHttpRequest();
    api::DataController controller;

    bool callbackCalled = false;
    drogon::HttpResponsePtr response;

    controller.handleStatus(
        req,
        [&](const drogon::HttpResponsePtr &r) {
            callbackCalled = true;
            response = r;
        },
        document.documentId);

    ASSERT_TRUE(callbackCalled);
    ASSERT_TRUE(response);
    EXPECT_EQ(response->getStatusCode(), drogon::k403Forbidden);
    EXPECT_NE(response->body().find("PERMISSION_DENIED"), std::string::npos);

    common::setTokenVerifier(common::devBypassVerifier);
}
