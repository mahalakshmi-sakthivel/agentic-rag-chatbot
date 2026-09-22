#include "MetadataExtractor.h"

namespace ingestion::metadata
{

models::Metadata MetadataExtractor::extract(
    const std::string &filename,
    const std::string &fileType,
    const std::string &tenantId,
    const std::string &ownerUserId)
{
    models::Metadata metadata;

    metadata.filename = filename;
    metadata.fileType = fileType;
    metadata.uploadedBy = ownerUserId;

    metadata.sourceLocation =
        "upload/" + tenantId + "/" + filename;

    metadata.tenantId = tenantId;
    metadata.ownerUserId = ownerUserId;

    return metadata;
}

} // namespace ingestion::metadata
