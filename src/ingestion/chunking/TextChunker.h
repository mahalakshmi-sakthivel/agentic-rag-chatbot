#pragma once

#include <string>
#include <vector>

namespace ingestion::chunking
{

class TextChunker
{
public:
    static std::vector<std::string> chunk(
        const std::string &text,
        std::size_t chunkSize,
        std::size_t overlap);
};

} // namespace ingestion::chunking
