/**
 * @file main.cpp
 * @brief Server entrypoint — wires auth layer into Crow HTTP server.
 *
 * Loads config from environment variables.
 * Runs DB migrations.
 * Registers authentication/authorization middleware and routes.
 */

#include <crow.h>
#include <iostream>
#include <stdexcept>

#include "config/auth_config.hpp"
#include "db/database.hpp"
#include "auth/auth_service.hpp"
#include "auth/auth_controller.hpp"
#include "auth/session_manager.hpp"
#include "middleware/authentication_middleware.hpp"
#include "middleware/authorization_middleware.hpp"

int main() {
    // ── Load configuration from environment ───────────────────────────────────
    auth::AuthConfig cfg;
    try {
        cfg = auth::AuthConfig::from_env();
    } catch (const std::exception& e) {
        std::cerr << "[startup] Configuration error: " << e.what() << "\n"
                  << "[startup] Copy .env.example to .env and set all required variables.\n";
        return 1;
    }

    // ── Open database and run migrations ─────────────────────────────────────
    auth::Database db(cfg.db_path);
    try {
        db.run_migrations("db/migrations");
        std::cout << "[startup] Migrations complete.\n";
    } catch (const std::exception& e) {
        std::cerr << "[startup] Migration error: " << e.what() << "\n";
        return 1;
    }

    // ── Construct services ────────────────────────────────────────────────────
    auth::AuthService    auth_svc(db, cfg);
    auth::SessionManager session_mgr(db);
    auth::TokenService   token_svc(cfg);

    // ── Construct middleware ──────────────────────────────────────────────────
    auth::middleware::AuthenticationMiddleware auth_middleware(token_svc, session_mgr);

    // ── Construct controller ──────────────────────────────────────────────────
    auth::AuthController controller(auth_svc, session_mgr);

    // ── Set up Crow app ───────────────────────────────────────────────────────
    crow::SimpleApp app;

    // Health check (public endpoint)
    CROW_ROUTE(app, "/health").methods(crow::HTTPMethod::GET)
    ([]() {
        crow::json::wvalue resp;
        resp["status"] = "ok";
        return crow::response(200, resp.dump());
    });

    // Register auth routes (POST /auth/register, POST /auth/login, etc.)
    controller.register_routes(app);

    // Example protected route — shows how Phase 1 handlers use the middleware
    CROW_ROUTE(app, "/api/protected").methods(crow::HTTPMethod::GET)
    ([&auth_middleware](crow::request& req, crow::response& res) {
        // ── Authentication middleware ──────────────────────────────────────
        if (!auth_middleware.authenticate(req, res)) {
            res.end(); // 401 already set
            return;
        }
        // ── Handler logic ─────────────────────────────────────────────────
        std::string user_id = req.get_header_value("X-Identity-User-Id");
        crow::json::wvalue body;
        body["message"] = "Protected resource accessed";
        body["user_id"] = user_id;
        res = crow::response(200, body.dump());
        res.set_header("Content-Type", "application/json");
        res.end();
    });

    // ── Start server ──────────────────────────────────────────────────────────
    const char* host = std::getenv("SERVER_HOST");
    const char* port = std::getenv("SERVER_PORT");
    uint16_t server_port = port ? static_cast<uint16_t>(std::stoi(port)) : 8080;

    std::cout << "[startup] Server starting on port " << server_port << "\n";
    app.bindaddr(host ? host : "0.0.0.0")
       .port(server_port)
       .multithreaded()
       .run();

    return 0;
}
