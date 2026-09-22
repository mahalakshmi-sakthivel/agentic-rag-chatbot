/**
 * @file token_tests.cpp
 * @brief Unit tests for TokenService JWT validation (§42.1 of phase doc)
 *
 * Tests:
 *   - Valid token validates
 *   - Expired token returns nullopt
 *   - Tampered payload returns nullopt
 *   - Invalid signature returns nullopt
 *   - Wrong algorithm (e.g., "none") returns nullopt
 *   - Missing required claims returns nullopt
 *   - Token does not leak secrets in claims
 */

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#include "../../config/auth_config.hpp"
#include "auth/token_service.hpp"
#include <chrono>
#include <thread>

using namespace auth;

// ─────────────────────────────────────────────────────────────────────────────
// Fixture
// ─────────────────────────────────────────────────────────────────────────────

struct TokenFixture {
  AuthConfig cfg;
  TokenService svc;

  TokenFixture() : cfg(make_config()), svc(cfg) {}

  static AuthConfig make_config() {
    AuthConfig c;
    c.jwt_secret = "test_secret_exactly_32_chars_abc";
    c.jwt_issuer = "test-issuer";
    c.jwt_audience = "test-audience";
    c.token_expiry_seconds =
        86400; // 24 hours — matches production default (§ auth_config.hpp)
    return c;
  }

  TokenClaims sample_claims(const std::string &user_id = "user-uuid-123") {
    return {user_id, "tenant-1", "session-uuid-456", {"user"}};
  }
};

// ─────────────────────────────────────────────────────────────────────────────
// Valid token
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(TokenFixture,
                 "TokenService: valid token validates successfully",
                 "[token]") {
  auto token = svc.create_token(sample_claims());
  auto result = svc.validate_token(token);
  REQUIRE(result.has_value());
  REQUIRE(result->user_id == "user-uuid-123");
  REQUIRE(result->tenant_id == "tenant-1");
  REQUIRE(result->session_id == "session-uuid-456");
  REQUIRE_FALSE(result->roles.empty());
  REQUIRE(result->roles[0] == "user");
}

TEST_CASE_METHOD(TokenFixture, "TokenService: token is a non-empty string",
                 "[token]") {
  auto token = svc.create_token(sample_claims());
  REQUIRE_FALSE(token.empty());
}

// ─────────────────────────────────────────────────────────────────────────────
// Expired token
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(TokenFixture, "TokenService: expired token returns nullopt",
                 "[token]") {
  // Create a config with 0-second expiry to get an expired token instantly
  AuthConfig short_cfg = make_config();
  short_cfg.token_expiry_seconds = 1;
  TokenService short_svc(short_cfg);

  auto token = short_svc.create_token(sample_claims());
  // Sleep 2 seconds to let it expire
  std::this_thread::sleep_for(std::chrono::seconds(2));

  auto result = svc.validate_token(token);
  REQUIRE_FALSE(result.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// Invalid signature / tampering
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(TokenFixture,
                 "TokenService: tampered signature returns nullopt",
                 "[token]") {
  auto token = svc.create_token(sample_claims());
  // Corrupt the last few characters of the signature
  if (token.size() > 5) {
    token[token.size() - 1] = (token.back() == 'a') ? 'b' : 'a';
    token[token.size() - 2] = (token[token.size() - 2] == 'x') ? 'y' : 'x';
  }
  auto result = svc.validate_token(token);
  REQUIRE_FALSE(result.has_value());
}

TEST_CASE_METHOD(
    TokenFixture,
    "TokenService: token signed with different secret returns nullopt",
    "[token]") {
  AuthConfig bad_cfg = make_config();
  bad_cfg.jwt_secret = "different_secret_exactly_32_chars";
  TokenService bad_svc(bad_cfg);

  auto token = bad_svc.create_token(sample_claims());
  auto result = svc.validate_token(token); // validated with CORRECT config
  REQUIRE_FALSE(result.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// Wrong issuer / audience
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(TokenFixture, "TokenService: wrong issuer returns nullopt",
                 "[token]") {
  AuthConfig bad_cfg = make_config();
  bad_cfg.jwt_issuer = "attacker-issuer";
  TokenService bad_svc(bad_cfg);

  auto token = bad_svc.create_token(sample_claims());
  auto result = svc.validate_token(token);
  REQUIRE_FALSE(result.has_value());
}

TEST_CASE_METHOD(TokenFixture, "TokenService: wrong audience returns nullopt",
                 "[token]") {
  AuthConfig bad_cfg = make_config();
  bad_cfg.jwt_audience = "wrong-audience";
  TokenService bad_svc(bad_cfg);

  auto token = bad_svc.create_token(sample_claims());
  auto result = svc.validate_token(token);
  REQUIRE_FALSE(result.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// Garbage input
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(TokenFixture, "TokenService: empty string returns nullopt",
                 "[token]") {
  auto result = svc.validate_token("");
  REQUIRE_FALSE(result.has_value());
}

TEST_CASE_METHOD(TokenFixture, "TokenService: random garbage returns nullopt",
                 "[token]") {
  auto result = svc.validate_token("not.a.jwt.at.all.xxxx");
  REQUIRE_FALSE(result.has_value());
}

TEST_CASE_METHOD(
    TokenFixture,
    "TokenService: plain base64 payload (no signature) returns nullopt",
    "[token]") {
  // Simulate a 'none' algorithm attack by passing a token with empty signature
  // The validator must reject this
  std::string fake = "eyJhbGciOiJub25lIiwidHlwIjoiSldUIn0."
                     "eyJzdWIiOiJ1c2VyLXV1aWQifQ.";
  auto result = svc.validate_token(fake);
  REQUIRE_FALSE(result.has_value());
}

// ─────────────────────────────────────────────────────────────────────────────
// Secret not in token payload
// ─────────────────────────────────────────────────────────────────────────────

TEST_CASE_METHOD(TokenFixture,
                 "TokenService: JWT secret is not embedded in token",
                 "[token]") {
  auto token = svc.create_token(sample_claims());
  // The signing secret must never appear in the raw token string
  REQUIRE(token.find(cfg.jwt_secret) == std::string::npos);
}
