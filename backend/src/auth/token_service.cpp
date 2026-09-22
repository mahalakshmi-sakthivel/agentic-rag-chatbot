/**
 * @file token_service.cpp
 *
 * Uses jwt-cpp (header-only): https://github.com/Thalhammer/jwt-cpp
 *
 * Algorithm: HS256 (HMAC-SHA256)
 * The "none" algorithm and all asymmetric algorithms are explicitly rejected.
 */

#include "token_service.hpp"

#include <chrono>
#include <jwt-cpp/jwt.h>
#include <sstream>
#include <stdexcept>

namespace auth {

namespace {
// Generate a UUID-like unique token ID (jti claim)
// For production, use a proper UUID library (e.g., libuuid)
std::string generate_jti() {
  // Simple approach using random hex — replace with uuid_generate in production
  std::ostringstream oss;
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFF);
  oss << std::hex << dist(gen) << "-" << dist(gen) << "-" << dist(gen) << "-"
      << dist(gen);
  return oss.str();
}
} // anonymous namespace

TokenService::TokenService(const AuthConfig &cfg) : cfg_(cfg) {}

std::string TokenService::create_token(const TokenClaims &claims) const {
  using namespace std::chrono;

  auto now = system_clock::now();
  auto expires = now + seconds(cfg_.token_expiry_seconds);

  // Build roles as a JSON array claim (using std::set for jwt-cpp)
  std::set<std::string> roles_set(claims.roles.begin(), claims.roles.end());

  auto token =
      jwt::create()
          .set_type("JWT")
          .set_algorithm("HS256")
          .set_issuer(cfg_.jwt_issuer)
          .set_audience(cfg_.jwt_audience)
          .set_subject(claims.user_id) // sub = user_id
          .set_issued_at(now)
          .set_expires_at(expires)
          .set_id(generate_jti()) // jti = unique token ID
          .set_payload_claim("tenant_id", jwt::claim(claims.tenant_id))
          .set_payload_claim("session_id", jwt::claim(claims.session_id))
          .set_payload_claim("roles", jwt::claim(roles_set))
          .set_payload_claim("token_use", jwt::claim(claims.token_use))
          .sign(jwt::algorithm::hs256{cfg_.jwt_secret});

  // NOTE: The token itself is NOT logged here — intentional.
  return token;
}

std::string
TokenService::create_refresh_token(const TokenClaims &claims) const {
  TokenClaims refresh_claims = claims;
  refresh_claims.token_use = "refresh";

  return create_token(refresh_claims);
}

std::optional<TokenClaims>
TokenService::validate_token(const std::string &raw_token) const noexcept {
  try {
    // ── Decode ────────────────────────────────────────────────────────────
    auto decoded = jwt::decode(raw_token);

    // ── Verify algorithm — reject "none" and unexpected algorithms ────────
    if (decoded.get_algorithm() != "HS256") {
      return std::nullopt;
    }

    // ── Build verifier — validates signature, expiry, issuer, audience ───
    auto verifier = jwt::verify()
                        .allow_algorithm(jwt::algorithm::hs256{cfg_.jwt_secret})
                        .with_issuer(cfg_.jwt_issuer)
                        .with_audience(cfg_.jwt_audience);

    verifier.verify(decoded); // throws on any verification failure

    // ── Validate required claims are present ─────────────────────────────
    if (!decoded.has_subject() || !decoded.has_expires_at() ||
        !decoded.has_issued_at() || !decoded.has_id()) {
      return std::nullopt;
    }

    // ── Extract claims ────────────────────────────────────────────────────
    TokenClaims claims;
    claims.user_id = decoded.get_subject();
    claims.tenant_id = decoded.get_payload_claim("tenant_id").as_string();
    claims.session_id = decoded.get_payload_claim("session_id").as_string();
    claims.token_use = decoded.get_payload_claim("token_use").as_string();

    // Parse JSON array of roles
    auto roles_set = decoded.get_payload_claim("roles").as_set();
    claims.roles.assign(roles_set.begin(), roles_set.end());

    return claims;

  } catch (...) {
    // Any failure → unauthenticated (fail closed)
    // NOTE: We do NOT log the raw token or exception details containing
    // credentials
    return std::nullopt;
  }
}

} // namespace auth
