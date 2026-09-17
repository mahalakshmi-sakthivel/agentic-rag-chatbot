// tests/api/test_upload_validation.cpp
//
// Section 14 + Section 11 (security baseline): file_type enum enforcement
// and filename sanitization, including path-traversal attempts.
//
#include "api/models/UploadModels.h"
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
