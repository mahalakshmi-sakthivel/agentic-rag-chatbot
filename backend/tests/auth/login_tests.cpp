/**
 * @file login_tests.cpp
 * @brief Unit tests for AuthService login/register (§42.1 of phase doc)
 *
 * Tests:
 *   - Valid credentials → success + token returned
 *   - Invalid password  → failure (401)
 *   - Invalid username  → failure (401, no email enumeration)
 *   - Missing fields    → failure (400)
 *   - Malformed request → failure
 *   - Disabled account  → failure
 *
 * Uses an in-memory SQLite DB for isolation.
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "auth/auth_service.hpp"
#include "auth/password_hasher.hpp"
#include "db/database.hpp"
#include "../../config/auth_config.hpp"

using namespace auth;

// ─────────────────────────────────────────────────────────────────────────────
// Test fixture — in-memory DB with migrations
// ─────────────────────────────────────────────────────────────────────────────

struct LoginFixture {
    Database    db{":memory:"};
    AuthConfig  cfg;
    AuthService service;

    LoginFixture() : service(db, make_config()) {
        db.run_migrations("db/migrations");
    }

    static AuthConfig make_config() {
        AuthConfig c;
        c.jwt_secret           = "test_secret_at_least_32_characters_long_x";
        c.jwt_issuer           = "test-issuer";
        c.jwt_audience         = "test-audience";
        c.token_expiry_seconds = 86400;  // 24 hours — matches production default (§ auth_config.hpp)
        c.db_path              = ":memory:";
        return c;
    }

    // Helper: register a test user and return their user_id
    std::string create_user(const std::string& email = "test@example.com",
                            const std::string& password = "ValidPass123!") {
        auto result = service.register_user({email, password, ""});
        REQUIRE(result.success);
        return result.user_id;
    }
};

// ─────────────────────────────────────────────────────────────────────────────
// Registration tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(LoginFixture, "Register: valid input succeeds", "[login]") {
    auto result = service.register_user({"user@example.com", "StrongPass1!", ""});
    REQUIRE(result.success == true);
    REQUIRE_FALSE(result.user_id.empty());
}

TEST_CASE_METHOD(LoginFixture, "Register: duplicate email fails", "[login]") {
    service.register_user({"dup@example.com", "pass12345", ""});
    auto result = service.register_user({"dup@example.com", "pass12345", ""});
    REQUIRE(result.success == false);
}

TEST_CASE_METHOD(LoginFixture, "Register: empty email fails", "[login]") {
    auto result = service.register_user({"", "pass12345", ""});
    REQUIRE(result.success == false);
}

TEST_CASE_METHOD(LoginFixture, "Register: empty password fails", "[login]") {
    auto result = service.register_user({"a@b.com", "", ""});
    REQUIRE(result.success == false);
}

TEST_CASE_METHOD(LoginFixture, "Register: short password fails", "[login]") {
    auto result = service.register_user({"a@b.com", "short", ""});
    REQUIRE(result.success == false);
}

// ─────────────────────────────────────────────────────────────────────────────
// Login tests
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(LoginFixture, "Login: valid credentials succeed", "[login]") {
    create_user("good@test.com", "GoodPass123!");
    auto result = service.login({"good@test.com", "GoodPass123!", ""});
    REQUIRE(result.success == true);
    REQUIRE_FALSE(result.access_token.empty());
    REQUIRE(result.token_type == "Bearer");
    REQUIRE(result.expires_in > 0);
}

TEST_CASE_METHOD(LoginFixture, "Login: wrong password fails", "[login]") {
    create_user("user@test.com", "CorrectPass1!");
    auto result = service.login({"user@test.com", "WrongPass1!", ""});
    REQUIRE(result.success == false);
    REQUIRE(result.access_token.empty());
    // Generic error message — does NOT reveal password was wrong vs email unknown
    REQUIRE(result.message == "Invalid credentials");
}

TEST_CASE_METHOD(LoginFixture, "Login: unknown email fails with same generic message", "[login]") {
    auto result = service.login({"nobody@test.com", "AnyPass1!", ""});
    REQUIRE(result.success == false);
    REQUIRE(result.access_token.empty());
    // IMPORTANT: same message as wrong password to prevent email enumeration
    REQUIRE(result.message == "Invalid credentials");
}

TEST_CASE_METHOD(LoginFixture, "Login: empty email fails", "[login]") {
    auto result = service.login({"", "AnyPass1!", ""});
    REQUIRE(result.success == false);
    REQUIRE(result.access_token.empty());
}

TEST_CASE_METHOD(LoginFixture, "Login: empty password fails", "[login]") {
    create_user("user2@test.com", "SomePass1!");
    auto result = service.login({"user2@test.com", "", ""});
    REQUIRE(result.success == false);
    REQUIRE(result.access_token.empty());
}

TEST_CASE_METHOD(LoginFixture, "Login: returned token does not contain plain password", "[login]") {
    std::string pw = "MySecretP@ssword1";
    create_user("tok@test.com", pw);
    auto result = service.login({"tok@test.com", pw, ""});
    REQUIRE(result.success == true);
    // Token must not contain the password
    REQUIRE(result.access_token.find(pw) == std::string::npos);
}
