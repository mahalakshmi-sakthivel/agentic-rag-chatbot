/**
 * @file password_hasher.cpp
 *
 * Uses libbcrypt (https://github.com/trusch/libbcrypt)
 * which wraps OpenBSD's bcrypt implementation.
 *
 * Linking: target_link_libraries(... bcrypt_lib)
 */

#include "password_hasher.hpp"

#include <stdexcept>
#include <string>

// libbcrypt header
#include "bcrypt/bcrypt.h"

namespace auth {

std::string PasswordHasher::hash(const std::string& password, int work_factor) {
    if (password.empty()) {
        throw std::invalid_argument("Password must not be empty");
    }
    if (work_factor < 4 || work_factor > 31) {
        throw std::invalid_argument("bcrypt work_factor must be between 4 and 31");
    }

    // NOTE: password is NOT logged here — intentional.
    // Step 1: generate a random salt with the requested work factor
    char salt[64] = {};
    int salt_result = bcrypt_gensalt(work_factor, salt);
    if (salt_result != 0) {
        throw std::runtime_error("bcrypt salt generation failed");
    }
    // Step 2: hash the password with the generated salt
    char hash_buf[64] = {};
    int result = bcrypt_hashpw(password.c_str(), salt, hash_buf);
    if (result != 0) {
        throw std::runtime_error("bcrypt hashing failed");
    }
    return std::string(hash_buf);
}

bool PasswordHasher::verify(const std::string& password,
                            const std::string& stored_hash) noexcept {
    if (password.empty() || stored_hash.empty()) {
        return false;
    }
    // NOTE: password is NOT logged here — intentional.
    // bcrypt_checkpw uses constant-time comparison
    return bcrypt_checkpw(password.c_str(), stored_hash.c_str()) == 0;
}

} // namespace auth
