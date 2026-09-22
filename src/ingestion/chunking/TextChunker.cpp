#include "TextChunker.h"

namespace ingestion::chunking
{

std::vector<std::string> TextChunker::chunk(
    const std::string &text,
    std::size_t chunkSize,
    std::size_t overlap)
{
    std::vector<std::string> chunks;

    if (text.empty() || chunkSize == 0)
    {
        return chunks;
    }

    if (overlap >= chunkSize)
    {
        overlap = 0;
    }

    std::size_t position = 0;

    while (position < text.size())
    {
        const std::size_t remaining =
            text.size() - position;

        const std::size_t length =
            remaining < chunkSize
                ? remaining
                : chunkSize;

        chunks.push_back(
            text.substr(position, length));

        if (position + length >= text.size())
        {
            break;
        }

        position += length - overlap;
    }

    return chunks;
}

} // namespace ingestion::chunking
