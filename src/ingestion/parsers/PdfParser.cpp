#include "PdfParser.h"

#include <poppler-document.h>
#include <poppler-page.h>

#include <memory>
#include <string>

namespace ingestion::parsers
{

ParseResult PdfParser::parse(const std::string &fileBytes)
{
    ParseResult result;

    if (fileBytes.empty())
    {
        result.errorCode    = "EMPTY_DOCUMENT";
        result.errorMessage = "PDF file is empty";
        return result;
    }

    // Magic-byte guard: PDF files must start with "%PDF"
    if (fileBytes.size() < 4 ||
        fileBytes[0] != '%' || fileBytes[1] != 'P' ||
        fileBytes[2] != 'D' || fileBytes[3] != 'F')
    {
        result.errorCode    = "PARSE_FAILURE";
        result.errorMessage = "File does not have a valid PDF signature";
        return result;
    }

    std::unique_ptr<poppler::document> document(
        poppler::document::load_from_raw_data(
            fileBytes.data(),
            fileBytes.size()));

    if (!document)
    {
        result.errorCode    = "PARSE_FAILURE";
        result.errorMessage = "Unable to parse PDF document";
        return result;
    }

    const int pageCount = document->pages();

    for (int pageIndex = 0; pageIndex < pageCount; ++pageIndex)
    {
        std::unique_ptr<poppler::page> page(
            document->create_page(pageIndex));

        if (!page)
            continue;

        poppler::ustring utext = page->text();
        auto utf8Text = utext.to_utf8();

        std::string pageText(utf8Text.begin(), utf8Text.end());

        // Strip leading/trailing whitespace from the page text.
        const auto first = pageText.find_first_not_of(" \t\r\n");
        if (first == std::string::npos)
            continue; // blank page — skip, don't emit empty unit

        pageText = pageText.substr(first);
        const auto last = pageText.find_last_not_of(" \t\r\n");
        if (last != std::string::npos)
            pageText = pageText.substr(0, last + 1);

        if (pageText.empty())
            continue;

        ParsedUnit unit;
        unit.text           = std::move(pageText);
        unit.sourceLocation = "page " + std::to_string(pageIndex + 1); // 1-indexed
        result.units.push_back(std::move(unit));
    }

    if (result.units.empty())
    {
        result.errorCode    = "EMPTY_DOCUMENT";
        result.errorMessage = "PDF contains no readable text";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace ingestion::parsers
