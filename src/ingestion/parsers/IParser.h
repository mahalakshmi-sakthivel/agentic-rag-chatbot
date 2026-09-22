#pragma once

#include <string>
#include <vector>

namespace ingestion::parsers
{

// A single extracted unit from a document: its text content and the
// structural location it came from (e.g. "page 3", "row 12", "$.orders[4]",
// "sheet 'Sheet1' row 5"). Phase 3 → Phase 4 Chunk Output Schema §11.3
// requires source_location per chunk; parsers supply it at this granularity.
struct ParsedUnit
{
    std::string text;           // raw extracted text for this unit
    std::string sourceLocation; // human-readable location within the document
};

struct ParseResult
{
    bool success = false;

    // Structured units with per-unit source locations.
    // Populated by all parsers for correct source_location propagation.
    std::vector<ParsedUnit> units;

    std::string errorCode;
    std::string errorMessage;
};

class IParser
{
public:
    virtual ~IParser() = default;

    // fileBytes: raw file content (already validated for size/type by Phase 1).
    // Returns ParseResult with units populated on success.
    virtual ParseResult parse(const std::string &fileBytes) = 0;
};

} // namespace ingestion::parsers
