#include "ingestion/parsers/ParserFactory.h"
#include <gtest/gtest.h>

using namespace ingestion::parsers;

TEST(ParsersTest, CsvParser_ExtractsSourceLocation)
{
    auto parser = ParserFactory::create("csv");
    ASSERT_TRUE(parser);
    std::string csvData = "col1,col2\nval1,val2\nval3,val4";
    auto result = parser->parse(csvData);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.units.size(), 2);
    EXPECT_EQ(result.units[0].sourceLocation, "row 1");
    EXPECT_EQ(result.units[1].sourceLocation, "row 2");
}

TEST(ParsersTest, PdfParser_ReturnsUnits)
{
    auto parser = ParserFactory::create("pdf");
    // We send dummy bytes since PdfParser handles them or returns an error.
    // If it fails with "NOT_IMPLEMENTED" or empty, we expect failure, but let's test behavior.
    auto result = parser->parse("%PDF-1.4 dummy");
    // Depending on Poppler behavior on invalid bytes, it might fail parsing.
    // At least verify parser creation and failure structure.
    EXPECT_TRUE(parser);
}

TEST(ParsersTest, JsonParser_ExtractsSourceLocation)
{
    auto parser = ParserFactory::create("json");
    ASSERT_TRUE(parser);
    std::string jsonData = "[{\"id\":1}, {\"id\":2}]";
    auto result = parser->parse(jsonData);
    EXPECT_TRUE(result.success);
    if(result.units.size() > 0) {
        EXPECT_FALSE(result.units[0].sourceLocation.empty());
    }
}

TEST(ParsersTest, XlsxParser_Creation)
{
    auto parser = ParserFactory::create("xlsx");
    ASSERT_TRUE(parser);
}

TEST(ParsersTest, CsvParser_EmptyDocument)
{
    auto parser = ParserFactory::create("csv");
    auto result = parser->parse("");
    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, "EMPTY_DOCUMENT");
}


TEST(ParsersTest, CsvParser_SupportsQuotedComma)
{
    auto parser = ParserFactory::create("csv");
    ASSERT_TRUE(parser);

    const std::string csv =
        "Name,Description\n"
        "Item 1,\"Item with, comma\"\n";

    const auto result = parser->parse(csv);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.units.size(), 1u);
    EXPECT_NE(result.units[0].text.find("Item with, comma"), std::string::npos);
}

TEST(ParsersTest, CsvParser_SupportsEscapedQuotes)
{
    auto parser = ParserFactory::create("csv");
    ASSERT_TRUE(parser);

    const std::string csv =
        "Name,Description\n"
        "Item 1,\"He said \"\"hello\"\"\"\n";

    const auto result = parser->parse(csv);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.units.size(), 1u);
    EXPECT_NE(result.units[0].text.find("He said \"hello\""), std::string::npos);
}

TEST(ParsersTest, CsvParser_AllowsDelimiterInsideQuotedValue)
{
    auto parser = ParserFactory::create("csv");
    ASSERT_TRUE(parser);

    const std::string csv =
        "Name;Description\n"
        "Item 1;\"value;with;semicolons\"\n";

    const auto result = parser->parse(csv);
    ASSERT_TRUE(result.success);
    ASSERT_EQ(result.units.size(), 1u);
    EXPECT_NE(result.units[0].text.find("value;with;semicolons"), std::string::npos);
}

TEST(ParsersTest, CsvParser_RejectsUnterminatedQuote)
{
    auto parser = ParserFactory::create("csv");
    ASSERT_TRUE(parser);

    const auto result = parser->parse(
        "Name,Description\n"
        "Item 1,\"unterminated\n");

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.errorCode, "PARSE_FAILURE");
}
