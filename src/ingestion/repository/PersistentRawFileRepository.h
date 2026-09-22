#pragma once

#include "RawFileRepository.h"

#include <filesystem>
#include <string>

namespace ingestion::repository
{

// Persistent raw-file repository for Phase 3. Files survive process restarts.
// The root directory is configurable with RAW_STORAGE_DIR and defaults to
// data/uploads. The repository never uses the client filename as a path.
class PersistentRawFileRepository : public RawFileRepository
{
public:
    explicit PersistentRawFileRepository(
        std::filesystem::path root = {});

    bool saveRawFile(
        const std::string &documentId,
        const std::string &tenantId,
        const std::string &filename,
        const std::string &fileBytes) override;

    bool deleteRawFile(
        const std::string &documentId,
        const std::string &tenantId) override;

private:
    std::filesystem::path root_;
};

} // namespace ingestion::repository
