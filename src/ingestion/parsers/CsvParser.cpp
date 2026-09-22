#include "CsvParser.h"

#include <algorithm>
#include <string>
#include <vector>

namespace ingestion::parsers
{

namespace
{

// Count delimiters outside quoted fields so a comma inside "..." does not
// affect delimiter detection.
char detectDelimiter(const std::string &firstRecord)
{
    const char candidates[] = {',', '\t', ';', '|'};
    char best = ',';
    int bestCount = -1;

    for (char candidate : candidates)
    {
        int count = 0;
        bool inQuotes = false;
        for (std::size_t i = 0; i < firstRecord.size(); ++i)
        {
            const char c = firstRecord[i];
            if (c == '"')
            {
                if (inQuotes && i + 1 < firstRecord.size() &&
                    firstRecord[i + 1] == '"')
                {
                    ++i; // escaped quote
                }
                else
                {
                    inQuotes = !inQuotes;
                }
            }
            else if (c == candidate && !inQuotes)
            {
                ++count;
            }
        }

        if (count > bestCount)
        {
            bestCount = count;
            best = candidate;
        }
    }

    return best;
}

// RFC 4180-style CSV record parser.
// Supports:
//   * quoted fields
//   * delimiters inside quoted fields
//   * escaped quotes ("")
//   * CRLF/LF line endings
//   * newlines inside quoted fields
bool parseCsvRecords(
    const std::string &input,
    char delim,
    std::vector<std::vector<std::string>> &records,
    std::string &error)
{
    std::vector<std::string> row;
    std::string field;
    bool inQuotes = false;

    for (std::size_t i = 0; i < input.size(); ++i)
    {
        const char c = input[i];

        if (inQuotes)
        {
            if (c == '"')
            {
                if (i + 1 < input.size() && input[i + 1] == '"')
                {
                    field.push_back('"');
                    ++i;
                }
                else
                {
                    inQuotes = false;
                }
            }
            else
            {
                field.push_back(c);
            }
            continue;
        }

        if (c == '"')
        {
            // Quotes are only valid at the beginning of a field in RFC 4180.
            if (!field.empty())
            {
                error = "Invalid quote in unquoted CSV field";
                return false;
            }
            inQuotes = true;
        }
        else if (c == delim)
        {
            row.push_back(field);
            field.clear();
        }
        else if (c == '\n' || c == '\r')
        {
            if (c == '\r' && i + 1 < input.size() && input[i + 1] == '\n')
                ++i;

            row.push_back(field);
            field.clear();

            if (!(row.size() == 1 && row[0].empty()))
                records.push_back(row);
            row.clear();
        }
        else
        {
            field.push_back(c);
        }
    }

    if (inQuotes)
    {
        error = "Unterminated quoted CSV field";
        return false;
    }

    // Flush a final record that does not end in a newline.
    if (!field.empty() || !row.empty())
    {
        row.push_back(field);
        if (!(row.size() == 1 && row[0].empty()))
            records.push_back(row);
    }

    return true;
}

// Build a human-readable "row N, columns: header=value, ..." representation.
std::string buildRowText(
    const std::vector<std::string> &headers,
    const std::vector<std::string> &values,
    int rowNumber)
{
    std::string out = "Row " + std::to_string(rowNumber) + ": ";
    for (std::size_t i = 0; i < values.size(); ++i)
    {
        if (i < headers.size() && !headers[i].empty())
            out += headers[i] + "=" + values[i];
        else
            out += "col" + std::to_string(i + 1) + "=" + values[i];

        if (i + 1 < values.size())
            out += ", ";
    }
    return out;
}

} // namespace

ParseResult CsvParser::parse(const std::string &fileBytes)
{
    ParseResult result;

    if (fileBytes.empty())
    {
        result.errorCode = "EMPTY_DOCUMENT";
        result.errorMessage = "CSV file is empty";
        return result;
    }

    // Use the first physical line only for delimiter detection. Quoted
    // delimiters are ignored by detectDelimiter().
    const auto firstLineEnd = fileBytes.find_first_of("\r\n");
    const std::string firstRecord =
        fileBytes.substr(0, firstLineEnd == std::string::npos
                              ? fileBytes.size()
                              : firstLineEnd);

    const char delim = detectDelimiter(firstRecord);

    std::vector<std::vector<std::string>> records;
    std::string parseError;
    if (!parseCsvRecords(fileBytes, delim, records, parseError))
    {
        result.errorCode = "PARSE_FAILURE";
        result.errorMessage = parseError;
        return result;
    }

    if (records.empty())
    {
        result.errorCode = "EMPTY_DOCUMENT";
        result.errorMessage = "CSV file contains no data rows";
        return result;
    }

    const std::vector<std::string> headers = records.front();
    int dataRowNum = 1;

    for (std::size_t recordIndex = 1; recordIndex < records.size(); ++recordIndex)
    {
        const auto &values = records[recordIndex];

        bool allBlank = true;
        for (const auto &value : values)
        {
            if (!value.empty())
            {
                allBlank = false;
                break;
            }
        }
        if (allBlank)
            continue;

        ParsedUnit unit;
        unit.text = buildRowText(headers, values, dataRowNum);
        unit.sourceLocation = "row " + std::to_string(dataRowNum);
        result.units.push_back(std::move(unit));
        ++dataRowNum;
    }

    if (result.units.empty())
    {
        result.errorCode = "EMPTY_DOCUMENT";
        result.errorMessage = "CSV file contains no data rows";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace ingestion::parsers
