#include "FileValidator.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace ingestion::validation
{

namespace
{

std::string getExtension(const std::string &filename)
{
    const auto position = filename.find_last_of('.');
    if (position == std::string::npos)
        return "";

    std::string ext = filename.substr(position + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

// --------------------------------------------------------------------------
// Magic-byte checks (§19.4 — file signature validation)
// --------------------------------------------------------------------------

bool isPdf(const std::string &bytes)
{
    return bytes.size() >= 4 &&
           bytes[0] == '%' && bytes[1] == 'P' &&
           bytes[2] == 'D' && bytes[3] == 'F';
}

// XLSX is a ZIP archive (PK header)
bool isZip(const std::string &bytes)
{
    return bytes.size() >= 4 &&
           static_cast<unsigned char>(bytes[0]) == 0x50 &&
           static_cast<unsigned char>(bytes[1]) == 0x4B &&
           static_cast<unsigned char>(bytes[2]) == 0x03 &&
           static_cast<unsigned char>(bytes[3]) == 0x04;
}

bool looksLikeTextUtf8(const std::string &bytes)
{
    // CSV and JSON should not contain null bytes or very high counts of
    // non-printable binary content. Heuristic: reject if > 10 % of the
    // first 1024 bytes are null or C0 control chars (excluding TAB/LF/CR).
    const std::size_t sample = std::min(bytes.size(), std::size_t{1024});
    int               suspicious = 0;

    for (std::size_t i = 0; i < sample; ++i)
    {
        const unsigned char c = static_cast<unsigned char>(bytes[i]);
        if (c == 0x00 || (c < 0x09) || (c == 0x0B || c == 0x0C) ||
            (c >= 0x0E && c < 0x20))
        {
            ++suspicious;
        }
    }

    return (suspicious * 10) <= static_cast<int>(sample); // ≤10%
}

// --------------------------------------------------------------------------
// Path-traversal safety (§19.4)
// --------------------------------------------------------------------------
bool hasPathTraversal(const std::string &filename)
{
    // Reject any path component that would escape the storage root.
    if (filename.find("..") != std::string::npos)
        return true;
    if (filename.find('/') != std::string::npos)
        return true;
    if (filename.find('\\') != std::string::npos)
        return true;
    if (!filename.empty() && filename[0] == '~')
        return true;
    return false;
}

// --------------------------------------------------------------------------
// Resource limit (§19.4 — max file size from env, default 50 MB)
// --------------------------------------------------------------------------
std::size_t maxFileSizeBytes()
{
    const char *env = std::getenv("MAX_UPLOAD_SIZE_MB");
    if (env && *env != '\0')
    {
        try
        {
            const int mb = std::stoi(env);
            if (mb > 0)
                return static_cast<std::size_t>(mb) * 1024 * 1024;
        }
        catch (...) {}
    }
    return 50ull * 1024 * 1024; // 50 MB default (§35 item 6)
}

} // namespace

ValidationResult FileValidator::validate(
    const std::string &filename,
    const std::string &fileType,
    const std::string &fileBytes)
{
    ValidationResult result;

    // 1. Filename must not be empty
    if (filename.empty())
    {
        result.errorCode    = "VALIDATION_ERROR";
        result.errorMessage = "Filename is empty";
        return result;
    }

    // 2. Filename path-traversal check (§19.4)
    if (hasPathTraversal(filename))
    {
        result.errorCode    = "VALIDATION_ERROR";
        result.errorMessage = "Filename contains unsafe path characters";
        return result;
    }

    // 3. File must not be empty
    if (fileBytes.empty())
    {
        result.errorCode    = "EMPTY_DOCUMENT";
        result.errorMessage = "Uploaded file is empty";
        return result;
    }

    // 4. Resource limit check (§19.4)
    if (fileBytes.size() > maxFileSizeBytes())
    {
        result.errorCode    = "FILE_TOO_LARGE";
        result.errorMessage = "Uploaded file exceeds the maximum allowed size";
        return result;
    }

    // 5. Extension must be in the supported set
    const std::string ext = getExtension(filename);
    if (ext != "pdf" && ext != "csv" && ext != "xlsx" && ext != "json")
    {
        result.errorCode    = "UNSUPPORTED_FILE_TYPE";
        result.errorMessage = "Only PDF, CSV, XLSX and JSON files are supported";
        return result;
    }

    // 6. Declared file_type must match filename extension
    if (!fileType.empty() && fileType != ext)
    {
        result.errorCode    = "VALIDATION_ERROR";
        result.errorMessage = "Declared file_type '" + fileType +
                              "' does not match file extension '." + ext + "'";
        return result;
    }

    // 7. Magic-byte / file-signature validation (§19.4)
    //    Defense-in-depth on top of the UploadController's extension check.
    if (ext == "pdf" || fileType == "pdf")
    {
        if (!isPdf(fileBytes))
        {
            result.errorCode    = "PARSE_FAILURE";
            result.errorMessage = "File does not have a valid PDF signature";
            return result;
        }
    }
    else if (ext == "xlsx" || fileType == "xlsx")
    {
        if (!isZip(fileBytes))
        {
            result.errorCode    = "PARSE_FAILURE";
            result.errorMessage = "File does not have a valid XLSX/ZIP signature";
            return result;
        }
    }
    else if (ext == "csv" || fileType == "csv")
    {
        if (!looksLikeTextUtf8(fileBytes))
        {
            result.errorCode    = "PARSE_FAILURE";
            result.errorMessage = "CSV file appears to contain binary/non-text content";
            return result;
        }
    }
    else if (ext == "json" || fileType == "json")
    {
        // First non-whitespace byte must be { or [
        const auto first = fileBytes.find_first_not_of(" \t\r\n");
        if (first == std::string::npos ||
            (fileBytes[first] != '{' && fileBytes[first] != '['))
        {
            result.errorCode    = "PARSE_FAILURE";
            result.errorMessage = "File does not appear to be valid JSON";
            return result;
        }
    }

    result.valid = true;
    return result;
}

} // namespace ingestion::validation
