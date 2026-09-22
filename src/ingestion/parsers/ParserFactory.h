#pragma once

#include "IParser.h"

#include <memory>
#include <string>

namespace ingestion::parsers
{

class ParserFactory
{
public:
    static std::unique_ptr<IParser> create(
        const std::string &fileType);
};

} // namespace ingestion::parsers
