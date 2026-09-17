#pragma once
/**
 * @file password_hasher.hpp
 * @brief Secure password hashing using bcrypt.
 *
 * Rules enforced:
 *  - Passwords are NEVER stored in plain text.
 *  - Passwords are NEVER logged.
 *  - Only the hash is stored in the database.
 *  - Verification is done via constant-time comparison (provided by bcrypt).
 */

#include <string>

namespace auth {

class PasswordHasher {
public:
    /**
     * @brief Hash a plain-text password using bcrypt.
     * @param password  The plain-text password. Must not be empty.
     * @param work_factor  bcrypt cost factor (4–31). Default 12 is safe for 2024+.
     * @return The bcrypt hash string (60 characters, includes salt).
     * @throws std::invalid_argument if password is empty.
     * @throws std::runtime_error    if hashing fails.
     */
    static std::string hash(const std::string& password, int work_factor = 12);

    /**
     * @brief Verify a plain-text password against a stored bcrypt hash.
     * @param password      The plain-text candidate password.
     * @param stored_hash   The previously computed bcrypt hash from the database.
     * @return true if the password matches, false otherwise.
     *
     * Uses constant-time comparison — safe against timing attacks.
     */
    static bool verify(const std::string& password,
                       const std::string& stored_hash) noexcept;

private:
    // No instances — all methods are static
    PasswordHasher() = delete;
};

} // namespace auth
