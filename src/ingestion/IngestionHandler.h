#pragma once

#include "api/controllers/UploadController.h"
#include <string>

namespace ingestion
{

api::IngestionResult handleIngestion(
    const api::IngestionRequest &request);

bool deleteRawFile(
    const std::string &documentId,
    const std::string &tenantId);

}