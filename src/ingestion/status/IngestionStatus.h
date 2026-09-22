#pragma once

#include <string>

namespace ingestion::status
{

// Contract §11.4 — exactly these four values, in this lifecycle order:
//   queued → processing → ready | failed
// No intermediate states (validating/parsing/cleaning/chunking) are contract-visible.
enum class IngestionStatus
{
    QUEUED,
    PROCESSING,
    READY,
    FAILED
};

inline std::string toString(IngestionStatus status)
{
    switch (status)
    {
    case IngestionStatus::QUEUED:
        return "queued";

    case IngestionStatus::PROCESSING:
        return "processing";

    case IngestionStatus::READY:
        return "ready";

    case IngestionStatus::FAILED:
        return "failed";
    }

    return "failed";
}

} // namespace ingestion::status
