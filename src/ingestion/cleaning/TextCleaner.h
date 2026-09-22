#pragma once

#include <string>

namespace ingestion::cleaning
{

class TextCleaner
{
public:
    static std::string clean(
        const std::string &text);
};

} // namespace ingestion::cleaning
