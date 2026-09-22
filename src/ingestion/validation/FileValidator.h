#pragma once

#include <string>

namespace ingestion::validation
{

struct ValidationResult
{
    bool valid = false;
    std::string errorCode;
    std::string errorMessage;
};

class FileValidator
{
public:
    static ValidationResult validate(
        const std::string &filename,
        const std::string &fileType,
        const std::string &fileBytes);
};

} // namespace ingestion::validation

