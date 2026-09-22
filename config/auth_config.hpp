#pragma once
/**
 * @file auth_config.hpp
 * @brief Authentication configuration loaded from environment variables.
 *
 * NEVER hardcode secrets. All sensitive values must be set as environment
 * variables before the process starts (e.g., via a .env loader or the OS).
 *
 * Required env vars:
 *   JWT_SECRET           – HMAC-SHA256 signing secret (min 32 chars)
 *   JWT_ISSUER           – Token issuer claim (e.g., "agentic-rag-chatbot")
 *   JWT_AUDIENCE         – Token audience claim (e.g., "chatbot-api")
 *   TOKEN_EXPIRY_SECONDS – Access-token lifetime in seconds (default: 86400 = 24 hours)
 *   DB_PATH              – Path to SQLite database file
 */

#include <cstdlib>
#include <string>
#include <stdexcept>

namespace auth {

struct AuthConfig {
    std::string jwt_secret;
    std::string jwt_issuer;
    std::string jwt_audience;
    int         token_expiry_seconds{86400};
    std::string db_path;

    /**
     * @brief Load configuration from environment variables.
     * @throws std::runtime_error if any required variable is missing or invalid.
     */
    static AuthConfig from_env() {
        AuthConfig cfg;

        // ── Required ─────────────────────────────────────────────────────────
        const char* secret = std::getenv("JWT_SECRET");
        if (!secret || std::string(secret).size() < 32) {
            throw std::runtime_error(
                "JWT_SECRET env var is missing or too short (min 32 chars)");
        }
        cfg.jwt_secret = secret;

        const char* issuer = std::getenv("JWT_ISSUER");
        if (!issuer) {
            throw std::runtime_error("JWT_ISSUER env var is missing");
        }
        cfg.jwt_issuer = issuer;

        const char* audience = std::getenv("JWT_AUDIENCE");
        if (!audience) {
            throw std::runtime_error("JWT_AUDIENCE env var is missing");
        }
        cfg.jwt_audience = audience;

        const char* db = std::getenv("DB_PATH");
        if (!db) {
            throw std::runtime_error("DB_PATH env var is missing");
        }
        cfg.db_path = db;

        // ── Optional with defaults ────────────────────────────────────────────
        const char* expiry = std::getenv("TOKEN_EXPIRY_SECONDS");
        if (expiry) {
            try {
                cfg.token_expiry_seconds = std::stoi(expiry);
                if (cfg.token_expiry_seconds <= 0) {
                    throw std::runtime_error(
                        "TOKEN_EXPIRY_SECONDS must be a positive integer");
                }
            } catch (const std::invalid_argument&) {
                throw std::runtime_error(
                    "TOKEN_EXPIRY_SECONDS must be a valid integer");
            }
        }

        return cfg;
    }
};

} // namespace auth
