// tests/api/test_upload_validation.cpp
//
// Section 14 + Section 11 (security baseline): file_type enum enforcement
// and filename sanitization, including path-traversal attempts.
//
#include "api/models/UploadModels.h"
#include <algorithm>
#include <gtest/gtest.h>

using namespace api::upload;

TEST(UploadValidation, AcceptsAllowedFileTypes)
{
    EXPECT_TRUE(isAllowedFileType("pdf"));
    EXPECT_TRUE(isAllowedFileType("csv"));
    EXPECT_TRUE(isAllowedFileType("xlsx"));
    EXPECT_TRUE(isAllowedFileType("json"));
}

TEST(UploadValidation, RejectsDisallowedFileType)
{
    EXPECT_FALSE(isAllowedFileType("exe"));
    EXPECT_FALSE(isAllowedFileType("PDF")); // case-sensitive per enum
    EXPECT_FALSE(isAllowedFileType(""));
}

TEST(FilenameSanitization, PassesThroughSimpleFilename)
{
    EXPECT_EQ(sanitizeFilename("report.pdf"), "report.pdf");
}

TEST(FilenameSanitization, StripsPathTraversalUnix)
{
    EXPECT_EQ(sanitizeFilename("../../etc/passwd"), "passwd");
}

TEST(FilenameSanitization, StripsPathTraversalWindows)
{
    EXPECT_EQ(sanitizeFilename("..\\..\\windows\\system32\\config"), "config");
}

TEST(FilenameSanitization, NeutralizesUnsafeCharacters)
{
    EXPECT_EQ(sanitizeFilename("weird name!@#.csv"), "weird_name___.csv");
}

TEST(FilenameSanitization, CollapsesLeadingDots)
{
    EXPECT_EQ(sanitizeFilename("...hidden"), "hidden");
}

TEST(FilenameSanitization, FallsBackToDefaultForEmptyResult)
{
    EXPECT_EQ(sanitizeFilename("...."), "unnamed_file");
    EXPECT_EQ(sanitizeFilename(""), "unnamed_file");
}

TEST(FilenameSanitization, HandlesVeryLongFilename)
{
    const std::string longName(500, 'a');
    const std::string result = sanitizeFilename(longName + ".csv");
    // Sanitization doesn't truncate length today — this test documents that
    // and gives Team Lead a concrete place to add a max-length clamp if
    // Section 11 wants one (filesystems generally cap at 255 bytes/segment).
    EXPECT_EQ(result, longName + ".csv");
}

TEST(FilenameSanitization, NeutralizesUnicodeCharacters)
{
    // Non-ASCII bytes are outside the isalnum allow-list and become '_',
    // same as any other unsafe byte — no crash, no path escape.
    const std::string result = sanitizeFilename("caf\xC3\xA9.csv");
    EXPECT_NE(result.find(".csv"), std::string::npos);
    EXPECT_EQ(result.find("\xC3\xA9"), std::string::npos);
}

// Checklist item 4 — string trimming (file_type).

TEST(UploadValidation, TrimHelperNormalizesFileType)
{
    EXPECT_EQ(trim("  pdf  "), "pdf");
    EXPECT_EQ(trim("csv"), "csv");
    EXPECT_EQ(trim("   "), "");
}

// Checklist item 3 — allowed multipart field allow-list, used by
// UploadController to reject unexpected fields instead of ignoring them.

TEST(UploadValidation, AllowedParamFieldsAreExactlyFileTypeAndMetadata)
{
    const auto &fields = allowedUploadParamFields();
    EXPECT_EQ(fields.size(), 2u);
    EXPECT_NE(std::find(fields.begin(), fields.end(), "file_type"), fields.end());
    EXPECT_NE(std::find(fields.begin(), fields.end(), "metadata"), fields.end());
}
