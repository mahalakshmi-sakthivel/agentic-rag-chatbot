#include "ChunkIdGenerator.h"

#include <openssl/sha.h>

#include <iomanip>
#include <sstream>
#include <string>

namespace ingestion::metadata
{

namespace
{
// RFC 4122 / RFC 9562-compatible UUID namespace UUID for this application's
// deterministic Phase 3 chunk names. The namespace is fixed so IDs remain
// stable across process restarts and machines.
constexpr unsigned char kNamespaceUuid[16] = {
    0x6b, 0xa7, 0xb8, 0x11, 0x9d, 0xad, 0x11, 0xd1,
    0x80, 0xb4, 0x00, 0xc0, 0x4f, 0xd4, 0x30, 0xc8
};

std::string formatUuid(const unsigned char bytes[16])
{
    std::ostringstream uuid;
    uuid << std::hex << std::setfill('0');

    for (int i = 0; i < 16; ++i)
    {
        if (i == 4 || i == 6 || i == 8 || i == 10)
            uuid << '-';

        uuid << std::setw(2) << static_cast<unsigned int>(bytes[i]);
    }

    return uuid.str();
}

} // namespace

std::string ChunkIdGenerator::generate(
    const std::string &documentId,
    int chunkIndex)
{
    // UUID v5 = SHA-1(namespace UUID || UTF-8 name), then set version/variant
    // bits. The name is deterministic for the document/chunk pair.
    const std::string name =
        documentId + ":" + std::to_string(chunkIndex);

    std::string input(
        reinterpret_cast<const char *>(kNamespaceUuid),
        sizeof(kNamespaceUuid));
    input.append(name);

    unsigned char digest[SHA_DIGEST_LENGTH];
    SHA1(
        reinterpret_cast<const unsigned char *>(input.data()),
        input.size(),
        digest);

    unsigned char uuid[16];
    for (int i = 0; i < 16; ++i)
        uuid[i] = digest[i];

    // Version 5.
    uuid[6] = static_cast<unsigned char>((uuid[6] & 0x0F) | 0x50);
    // RFC 4122 variant.
    uuid[8] = static_cast<unsigned char>((uuid[8] & 0x3F) | 0x80);

    return formatUuid(uuid);
}

} // namespace ingestion::metadata
