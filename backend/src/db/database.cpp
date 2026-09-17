/**
 * @file database.cpp
 *
 * SQLite3 wrapper implementation.
 * WAL mode is enabled for better concurrent read performance.
 * All queries are parameterized — no string formatting.
 */

#include "db/database.hpp"

#include <sqlite3.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <random>
#include <iomanip>
#include <algorithm>

namespace auth {

// ─────────────────────────────────────────────────────────────────────────────
// Construction / Destruction
// ─────────────────────────────────────────────────────────────────────────────

Database::Database(const std::string& db_path) {
    open(db_path);
}

Database::~Database() {
    close();
}

Database::Database(Database&& other) noexcept : db_(other.db_) {
    other.db_ = nullptr;
}

Database& Database::operator=(Database&& other) noexcept {
    if (this != &other) {
        close();
        db_ = other.db_;
        other.db_ = nullptr;
    }
    return *this;
}

// ─────────────────────────────────────────────────────────────────────────────
// execute
// ─────────────────────────────────────────────────────────────────────────────

void Database::execute(const std::string& sql,
                       const std::vector<std::string>& params) {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("DB prepare error: ") + sqlite3_errmsg(db_));
    }

    // Bind parameters (1-indexed in SQLite)
    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("DB bind error");
        }
    }

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
        throw std::runtime_error(std::string("DB execute error: ") + sqlite3_errmsg(db_));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// query
// ─────────────────────────────────────────────────────────────────────────────

std::vector<Row> Database::query(const std::string& sql,
                                  const std::vector<std::string>& params) const {
    sqlite3_stmt* stmt = nullptr;
    int rc = sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("DB prepare error: ") + sqlite3_errmsg(db_));
    }

    for (int i = 0; i < static_cast<int>(params.size()); ++i) {
        rc = sqlite3_bind_text(stmt, i + 1, params[i].c_str(), -1, SQLITE_TRANSIENT);
        if (rc != SQLITE_OK) {
            sqlite3_finalize(stmt);
            throw std::runtime_error("DB bind error");
        }
    }

    std::vector<Row> rows;
    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        Row row;
        int col_count = sqlite3_column_count(stmt);
        for (int c = 0; c < col_count; ++c) {
            std::string col_name = sqlite3_column_name(stmt, c);
            const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, c));
            row[col_name] = val ? val : "";
        }
        rows.push_back(std::move(row));
    }

    sqlite3_finalize(stmt);

    if (rc != SQLITE_DONE) {
        throw std::runtime_error(std::string("DB query error: ") + sqlite3_errmsg(db_));
    }

    return rows;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

int Database::last_changes() const noexcept {
    return sqlite3_changes(db_);
}

std::string Database::generate_uuid() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist;
    auto v1 = dist(gen);
    auto v2 = dist(gen);

    // Set version (4) and variant bits
    v1 = (v1 & 0xFFFFFFFFFFFF0FFFULL) | 0x0000000000004000ULL;
    v2 = (v2 & 0x3FFFFFFFFFFFFFFFULL) | 0x8000000000000000ULL;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0')
        << std::setw(8)  << (v1 >> 32)               << '-'
        << std::setw(4)  << ((v1 >> 16) & 0xFFFF)     << '-'
        << std::setw(4)  << (v1 & 0xFFFF)              << '-'
        << std::setw(4)  << (v2 >> 48)                 << '-'
        << std::setw(12) << (v2 & 0x0000FFFFFFFFFFFFULL);
    return oss.str();
}

void Database::run_migrations(const std::string& migrations_dir) {
    namespace fs = std::filesystem;

    // Collect and sort .sql files
    std::vector<fs::path> sql_files;
    for (const auto& entry : fs::directory_iterator(migrations_dir)) {
        if (entry.path().extension() == ".sql") {
            sql_files.push_back(entry.path());
        }
    }
    std::sort(sql_files.begin(), sql_files.end());

    for (const auto& path : sql_files) {
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open migration: " + path.string());
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        std::string migration_sql = ss.str();

        char* err_msg = nullptr;
        int rc = sqlite3_exec(db_, migration_sql.c_str(), nullptr, nullptr, &err_msg);
        if (rc != SQLITE_OK) {
            std::string err = err_msg ? err_msg : "unknown";
            sqlite3_free(err_msg);
            throw std::runtime_error("Migration failed (" + path.string() + "): " + err);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Private
// ─────────────────────────────────────────────────────────────────────────────

void Database::open(const std::string& path) {
    // Create parent directory if needed
    namespace fs = std::filesystem;
    fs::path db_file(path);
    if (db_file.has_parent_path()) {
        fs::create_directories(db_file.parent_path());
    }

    int rc = sqlite3_open(path.c_str(), &db_);
    if (rc != SQLITE_OK) {
        throw std::runtime_error(std::string("Cannot open database: ") +
                                 sqlite3_errmsg(db_));
    }
    enable_wal_mode();
    set_busy_timeout(5000);
}

void Database::close() noexcept {
    if (db_) {
        sqlite3_close_v2(db_);
        db_ = nullptr;
    }
}

void Database::enable_wal_mode() {
    sqlite3_exec(db_, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "PRAGMA foreign_keys=ON;",  nullptr, nullptr, nullptr);
}

void Database::set_busy_timeout(int ms) {
    sqlite3_busy_timeout(db_, ms);
}

} // namespace auth
