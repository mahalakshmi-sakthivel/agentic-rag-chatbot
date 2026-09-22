#pragma once

#include <string>

namespace ingestion::metadata
{

class ChunkIdGenerator
{
public:
    static std::string generate(
        const std::string &documentId,
        int chunkIndex);
};

} // namespace ingestion::metadata
