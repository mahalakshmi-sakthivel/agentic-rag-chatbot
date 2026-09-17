/**
 * @file password_tests.cpp
 * @brief Unit tests for PasswordHasher (§42.2 of phase doc)
 *
 * Tests:
 *   - Hash is never plain text
 *   - Correct password verifies
 *   - Wrong password is rejected
 *   - Empty inputs handled safely
 *   - Hash is not present in any output we control
 *
 * Framework: Catch2 (header-only)
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "auth/password_hasher.hpp"
#include <string>
#include <sstream>
#include <streambuf>

using namespace auth;

// ─────────────────────────────────────────────────────────────────────────────
// Hashing
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("PasswordHasher: hash is not the original password", "[password]") {
    std::string password = "correct_horse_battery_staple";
    std::string hash     = PasswordHasher::hash(password);

    REQUIRE(hash != password);
    REQUIRE_FALSE(hash.empty());
}

TEST_CASE("PasswordHasher: hash starts with bcrypt prefix", "[password]") {
    std::string hash = PasswordHasher::hash("testpassword123");
    // bcrypt hashes start with $2b$ or $2a$
    REQUIRE((hash.substr(0, 4) == "$2b$" || hash.substr(0, 4) == "$2a$"));
}

TEST_CASE("PasswordHasher: same password produces different hashes (salt)", "[password]") {
    // bcrypt auto-generates a random salt — two hashes of same password differ
    std::string pw   = "samepassword";
    std::string h1   = PasswordHasher::hash(pw);
    std::string h2   = PasswordHasher::hash(pw);
    REQUIRE(h1 != h2);
}

TEST_CASE("PasswordHasher: hash length is 60 chars (standard bcrypt)", "[password]") {
    std::string hash = PasswordHasher::hash("anypassword");
    REQUIRE(hash.size() == 60);
}

// ─────────────────────────────────────────────────────────────────────────────
// Verification
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("PasswordHasher: verify — correct password returns true", "[password]") {
    std::string pw   = "correct_horse_battery_staple";
    std::string hash = PasswordHasher::hash(pw);
    REQUIRE(PasswordHasher::verify(pw, hash) == true);
}

TEST_CASE("PasswordHasher: verify — wrong password returns false", "[password]") {
    std::string hash = PasswordHasher::hash("correct_password");
    REQUIRE(PasswordHasher::verify("wrong_password", hash) == false);
}

TEST_CASE("PasswordHasher: verify — similar but different password returns false", "[password]") {
    std::string hash = PasswordHasher::hash("password123");
    REQUIRE(PasswordHasher::verify("Password123", hash) == false);  // case-sensitive
    REQUIRE(PasswordHasher::verify("password124", hash) == false);
    REQUIRE(PasswordHasher::verify("password12",  hash) == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Edge cases
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("PasswordHasher: empty password throws on hash", "[password]") {
    REQUIRE_THROWS_AS(PasswordHasher::hash(""), std::invalid_argument);
}

TEST_CASE("PasswordHasher: verify with empty password returns false (no throw)", "[password]") {
    std::string hash = PasswordHasher::hash("realpassword");
    REQUIRE(PasswordHasher::verify("", hash) == false);
}

TEST_CASE("PasswordHasher: verify with empty hash returns false (no throw)", "[password]") {
    REQUIRE(PasswordHasher::verify("somepassword", "") == false);
}

TEST_CASE("PasswordHasher: verify with tampered hash returns false", "[password]") {
    std::string hash = PasswordHasher::hash("password");
    // Tamper one character
    hash[10] = (hash[10] == 'a') ? 'b' : 'a';
    REQUIRE(PasswordHasher::verify("password", hash) == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Ensure password does not appear in hash
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE("PasswordHasher: plain text password does not appear in hash string", "[password]") {
    std::string pw   = "MySecretP@ssw0rd";
    std::string hash = PasswordHasher::hash(pw);
    // The hash must not contain the original password as a substring
    REQUIRE(hash.find(pw) == std::string::npos);
}
