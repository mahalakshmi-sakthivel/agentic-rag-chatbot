// tests/api/test_ingestion_seam.cpp
//
// Section 11.3 (Chunk/handoff-adjacent contract) + UploadController.h's
// IngestionRequest/IngestionResult/ingestionHandler seam. Pure logic tests
// against the default placeholder (defaultIngestionStub) and the swap
// mechanism itself — no live server, no multipart parsing involved.
//
#include "api/controllers/UploadController.h"
#include "common/error_codes.h"
#include <gtest/gtest.h>

using namespace api;

namespace
{
IngestionRequest sampleRequest()
{
    IngestionRequest req;
    req.fileBytes = "%PDF-1.4 fake content";
    req.filename = "report.pdf";
    req.fileType = "pdf";
    req.metadata = nullptr;
    req.identity.user_id = "user-1";
    req.identity.tenant_id = "tenant-1";
    req.identity.roles = {"user"};
    req.identity.session_id = "session-1";
    req.identity.authenticated = true;
    return req;
}
} // namespace

// Default handler (active before Phase 3 assigns its own) must return a
// shape-correct success — non-empty document_id, status "queued" — so
// UploadController can build a valid Section 8.2 response with no Phase 3
// code linked in yet.
TEST(IngestionSeam, DefaultHandlerReturnsQueuedPlaceholder)
{
    const auto result = UploadController::ingestionHandler(sampleRequest());
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.documentId.empty());
    EXPECT_EQ(result.status, "queued");
}

// Each call gets its own document_id — Phase 8/UI-facing tests and any
// concurrent uploads must never collide on the placeholder.
TEST(IngestionSeam, DefaultHandlerGeneratesUniqueDocumentIds)
{
    const auto first = UploadController::ingestionHandler(sampleRequest());
    const auto second = UploadController::ingestionHandler(sampleRequest());
    EXPECT_NE(first.documentId, second.documentId);
}

// The seam itself: Phase 3 (or a test) can swap the handler exactly like
// QueryController::queryHandler, and UploadController.cpp always calls
// through UploadController::ingestionHandler rather than a hardcoded
// function — this is what "no controller changes needed" depends on.
TEST(IngestionSeam, HandlerIsSwappable)
{
    const auto original = UploadController::ingestionHandler;

    UploadController::ingestionHandler = [](const IngestionRequest &req) {
        IngestionResult result;
        result.success = true;
        result.documentId = "fixed-doc-id";
        result.status = "ready";
        EXPECT_EQ(req.fileType, "pdf");
        return result;
    };

    const auto result = UploadController::ingestionHandler(sampleRequest());
    EXPECT_EQ(result.documentId, "fixed-doc-id");
    EXPECT_EQ(result.status, "ready");

    UploadController::ingestionHandler = original; // restore for other tests
}

// A Phase 3 failure (Section 11.3 error conditions) must be representable
// and round-trip through the documented error codes.
TEST(IngestionSeam, HandlerCanReportSection11ErrorConditions)
{
    const auto original = UploadController::ingestionHandler;

    UploadController::ingestionHandler = [](const IngestionRequest &) {
        IngestionResult result;
        result.success = false;
        result.errorCode = common::ErrorCode::PARSE_FAILURE;
        result.errorMessage = "Could not parse file content";
        return result;
    };

    const auto result = UploadController::ingestionHandler(sampleRequest());
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, common::ErrorCode::PARSE_FAILURE);
    EXPECT_EQ(common::httpStatusForErrorCode(result.errorCode), 422);

    UploadController::ingestionHandler = original; // restore for other tests
}
