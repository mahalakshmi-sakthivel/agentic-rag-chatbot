#include "JsonParser.h"

#include <nlohmann/json.hpp>
#include <string>

namespace ingestion::parsers
{

namespace
{

// Recursively flatten a JSON value into leaf-level ParsedUnits.
// path:   JSONPath string accumulated so far (e.g. "$.orders[4].amount")
// units:  destination vector
// depth:  current recursion depth (guards against pathological nesting)
void flattenJson(
    const nlohmann::json    &value,
    const std::string       &path,
    std::vector<ParsedUnit> &units,
    int                      depth = 0)
{
    // Guard: very deeply nested structures are summarized rather than
    // expanded infinitely — keeps chunk count manageable.
    constexpr int kMaxDepth = 20;

    if (value.is_object())
    {
        if (value.empty())
        {
            ParsedUnit unit;
            unit.text           = path + ": {}";
            unit.sourceLocation = path;
            units.push_back(std::move(unit));
            return;
        }

        for (const auto &[key, child] : value.items())
        {
            const std::string childPath = path + "." + key;
            if (depth < kMaxDepth)
                flattenJson(child, childPath, units, depth + 1);
            else
            {
                // Summarize: dump the subtree as a single unit
                ParsedUnit unit;
                unit.text           = childPath + ": " + child.dump();
                unit.sourceLocation = childPath;
                units.push_back(std::move(unit));
            }
        }
    }
    else if (value.is_array())
    {
        if (value.empty())
        {
            ParsedUnit unit;
            unit.text           = path + ": []";
            unit.sourceLocation = path;
            units.push_back(std::move(unit));
            return;
        }

        for (std::size_t i = 0; i < value.size(); ++i)
        {
            const std::string elemPath =
                path + "[" + std::to_string(i) + "]";
            if (depth < kMaxDepth)
                flattenJson(value[i], elemPath, units, depth + 1);
            else
            {
                ParsedUnit unit;
                unit.text           = elemPath + ": " + value[i].dump();
                unit.sourceLocation = elemPath;
                units.push_back(std::move(unit));
            }
        }
    }
    else
    {
        // Leaf value: string, number, bool, null
        std::string text;
        if (value.is_string())
            text = path + ": " + value.get<std::string>();
        else
            text = path + ": " + value.dump();

        ParsedUnit unit;
        unit.text           = std::move(text);
        unit.sourceLocation = path;
        units.push_back(std::move(unit));
    }
}

} // namespace

ParseResult JsonParser::parse(const std::string &fileBytes)
{
    ParseResult result;

    if (fileBytes.empty())
    {
        result.errorCode    = "EMPTY_DOCUMENT";
        result.errorMessage = "JSON file is empty";
        return result;
    }

    // Basic magic-byte check: first non-whitespace char must be { or [
    const auto firstChar = fileBytes.find_first_not_of(" \t\r\n");
    if (firstChar == std::string::npos ||
        (fileBytes[firstChar] != '{' && fileBytes[firstChar] != '['))
    {
        result.errorCode    = "PARSE_FAILURE";
        result.errorMessage = "File does not appear to be valid JSON";
        return result;
    }

    nlohmann::json parsed;
    try
    {
        parsed = nlohmann::json::parse(fileBytes);
    }
    catch (const nlohmann::json::parse_error &)
    {
        result.errorCode    = "PARSE_FAILURE";
        result.errorMessage = "Invalid JSON document";
        return result;
    }

    // Flatten from the root using JSONPath notation ("$" = root)
    flattenJson(parsed, "$", result.units);

    if (result.units.empty())
    {
        result.errorCode    = "EMPTY_DOCUMENT";
        result.errorMessage = "JSON document contains no extractable content";
        return result;
    }

    result.success = true;
    return result;
}

} // namespace ingestion::parsers
