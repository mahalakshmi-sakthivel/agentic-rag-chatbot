#pragma once

#include <string>

namespace ingestion::models
{

struct Metadata
{
    std::string filename;
    std::string fileType;
    std::string uploadedBy;
    std::string sourceLocation;

    std::string tenantId;
    std::string ownerUserId;
};

} // namespace ingestion::models
