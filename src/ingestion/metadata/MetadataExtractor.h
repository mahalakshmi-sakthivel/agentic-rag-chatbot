#pragma once

#include <string>

#include "ingestion/models/Metadata.h"

namespace ingestion::metadata
{

class MetadataExtractor
{
public:
    static models::Metadata extract(
        const std::string &filename,
        const std::string &fileType,
        const std::string &tenantId,
        const std::string &ownerUserId);
};

} // namespace ingestion::metadata
