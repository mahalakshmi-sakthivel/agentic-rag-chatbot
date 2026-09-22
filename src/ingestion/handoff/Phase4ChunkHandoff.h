#pragma once

#include "ingestion/models/Chunk.h"
#include <string>
#include <vector>

namespace ingestion::handoff
{

// Stable Phase 3 -> Phase 4 boundary.
// Phase 4 consumes the exact Chunk schema produced by Phase 3; it must not
// depend on the concrete storage implementation.
class IPhase4ChunkProvider
{
public:
    virtual ~IPhase4ChunkProvider() = default;

    virtual std::vector<models::Chunk> getChunks(
        const std::string &documentId,
        const std::string &tenantId) = 0;
};

} // namespace ingestion::handoff
