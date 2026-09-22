#include "ParserFactory.h"

#include "CsvParser.h"
#include "JsonParser.h"
#include "PdfParser.h"
#include "XlsxParser.h"

namespace ingestion::parsers
{

std::unique_ptr<IParser> ParserFactory::create(
    const std::string &fileType)
{
    if (fileType == "pdf")
        return std::make_unique<PdfParser>();

    if (fileType == "csv")
        return std::make_unique<CsvParser>();

    if (fileType == "xlsx")
        return std::make_unique<XlsxParser>();

    if (fileType == "json")
        return std::make_unique<JsonParser>();

    return nullptr;
}

} // namespace ingestion::parsers
