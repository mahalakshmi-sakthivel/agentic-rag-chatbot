// common/uuid.h
//
// Small self-contained UUID v4 generator. No external dependency needed for
// Phase 1 — if a later phase wants a stronger source (e.g. libuuid), swap
// the implementation here; every caller only depends on generateUuid().
//
#pragma once

#include <random>
#include <sstream>
#include <string>

namespace common
{

inline std::string generateUuid()
{
    static thread_local std::mt19937_64 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 15);
    static const char *hex = "0123456789abcdef";

    std::string uuid(36, ' ');
    static const bool dash[36] = {
        false, false, false, false, false, false, false, false,
        true, false, false, false, false, true, false, false,
        false, false, true, false, false, false, false, true,
        false, false, false, false, false, false, false, false,
        false, false, false, false};

    for (int i = 0; i < 36; ++i)
    {
        if (dash[i])
        {
            uuid[i] = '-';
            continue;
        }
        if (i == 14)
        {
            uuid[i] = '4'; // version 4
            continue;
        }
        if (i == 19)
        {
            // variant bits: 8, 9, a, or b
            uuid[i] = hex[8 + (dist(rng) & 0x3)];
            continue;
        }
        uuid[i] = hex[dist(rng)];
    }
    return uuid;
}

} // namespace common
