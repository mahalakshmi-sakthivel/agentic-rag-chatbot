#pragma once

#include <string>

namespace ingestion::repository
{

class RawFileRepository
{
public:
    virtual ~RawFileRepository() = default;

    virtual bool saveRawFile(
        const std::string &documentId,
        const std::string &tenantId,
        const std::string &filename,
        const std::string &fileBytes) = 0;

    virtual bool deleteRawFile(
        const std::string &documentId,
        const std::string &tenantId) = 0;
};

} // namespace ingestion::repository
