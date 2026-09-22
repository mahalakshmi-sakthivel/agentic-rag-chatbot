#pragma once

#include "IParser.h"

namespace ingestion::parsers
{

class JsonParser : public IParser
{
public:
    ParseResult parse(
        const std::string &fileBytes) override;
};

} // namespace ingestion::parsers

