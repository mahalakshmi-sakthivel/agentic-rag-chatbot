#pragma once
/**
 * @file database.hpp
 * @brief Thin SQLite3 database wrapper used by the auth layer.
 *
 * Provides simple query/execute/transaction API.
 * All SQL is parameterized — no string concatenation → no SQL injection.
 */

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <stdexcept>

// SQLite3 C API
#include <sqlite3.h>

namespace auth {

using Row = std::unordered_map<std::string, std::string>;

class Database {
public:
    explicit Database(const std::string& db_path);
    ~Database();

    // Non-copyable, movable
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) noexcept;
    Database& operator=(Database&&) noexcept;

    /**
     * @brief Execute a parameterized SQL statement (INSERT/UPDATE/DELETE).
     * @param sql     Parameterized SQL with ? placeholders.
     * @param params  Parameter values in order.
     * @throws std::runtime_error on failure.
     */
    void execute(const std::string& sql,
                 const std::vector<std::string>& params = {});

    /**
     * @brief Execute a SELECT query and return rows.
     * @param sql     Parameterized SQL with ? placeholders.
     * @param params  Parameter values in order.
     * @return Vector of rows (each row = column_name → value map).
     * @throws std::runtime_error on failure.
     */
    std::vector<Row> query(const std::string& sql,
                           const std::vector<std::string>& params = {}) const;

    /**
     * @brief Returns number of rows changed by last execute().
     */
    int last_changes() const noexcept;

    /**
     * @brief Generate a UUID v4 string (helper used by services).
     */
    static std::string generate_uuid();

    /**
     * @brief Run all SQL migration files in order.
     * @param migrations_dir  Path to directory containing .sql migration files.
     */
    void run_migrations(const std::string& migrations_dir);

private:
    sqlite3* db_{nullptr};

    void open(const std::string& path);
    void close() noexcept;
    void enable_wal_mode();
    void set_busy_timeout(int ms);
};

} // namespace auth
