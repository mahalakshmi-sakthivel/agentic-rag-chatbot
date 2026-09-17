/**
 * @file session_manager.cpp
 */

#include "session_manager.hpp"

#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <stdexcept>

namespace auth {

namespace {
    std::string make_uuid() {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;
        std::ostringstream oss;
        auto v1 = dist(gen);
        auto v2 = dist(gen);
        // Format as 8-4-4-4-12 hex
        oss << std::hex << std::setfill('0')
            << std::setw(8) << (v1 >> 32)          << '-'
            << std::setw(4) << ((v1 >> 16) & 0xFFFF) << '-'
            << std::setw(4) << (v1 & 0xFFFF)         << '-'
            << std::setw(4) << (v2 >> 48)             << '-'
            << std::setw(12) << (v2 & 0xFFFFFFFFFFFF);
        return oss.str();
    }

    std::string tp_to_iso(const std::chrono::system_clock::time_point& tp) {
        std::time_t t = std::chrono::system_clock::to_time_t(tp);
        std::ostringstream oss;
        oss << std::put_time(std::gmtime(&t), "%Y-%m-%dT%H:%M:%SZ");
        return oss.str();
    }

    std::chrono::system_clock::time_point iso_to_tp(const std::string& s) {
        std::tm tm{};
        std::istringstream ss(s);
        ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
        return std::chrono::system_clock::from_time_t(std::mktime(&tm));
    }
} // anonymous namespace

SessionManager::SessionManager(Database& db) : db_(db) {}

std::string SessionManager::create_session(const std::string& user_id, int expiry_secs) {
    auto now     = std::chrono::system_clock::now();
    auto expires = now + std::chrono::seconds(expiry_secs);
    std::string sid = make_uuid();

    db_.execute(
        "INSERT INTO sessions (id, user_id, created_at, expires_at, status) "
        "VALUES (?, ?, ?, ?, 'active')",
        {sid, user_id, tp_to_iso(now), tp_to_iso(expires)}
    );

    return sid;
}

std::optional<Session> SessionManager::validate_session(
    const std::string& session_id) const noexcept
{
    try {
        auto rows = db_.query(
            "SELECT id, user_id, created_at, expires_at, status "
            "FROM sessions WHERE id = ? AND status = 'active'",
            {session_id}
        );

        if (rows.empty()) return std::nullopt;

        auto& row = rows[0];
        Session s;
        s.id         = row.at("id");
        s.user_id    = row.at("user_id");
        s.created_at = iso_to_tp(row.at("created_at"));
        s.expires_at = iso_to_tp(row.at("expires_at"));
        s.status     = SessionStatus::ACTIVE;

        // Enforce expiry even if DB wasn't cleaned up
        if (std::chrono::system_clock::now() >= s.expires_at) {
            return std::nullopt;
        }

        return s;
    } catch (...) {
        return std::nullopt;
    }
}

bool SessionManager::revoke_session(const std::string& session_id,
                                    const std::string& user_id) noexcept {
    try {
        // Ownership check: only revoke if session belongs to the requesting user
        db_.execute(
            "UPDATE sessions SET status = 'revoked' "
            "WHERE id = ? AND user_id = ? AND status = 'active'",
            {session_id, user_id}
        );
        return db_.last_changes() > 0;
    } catch (...) {
        return false;
    }
}

void SessionManager::revoke_all_sessions(const std::string& user_id) noexcept {
    try {
        db_.execute(
            "UPDATE sessions SET status = 'revoked' "
            "WHERE user_id = ? AND status = 'active'",
            {user_id}
        );
    } catch (...) {}
}

std::vector<Session> SessionManager::list_sessions(const std::string& user_id) const {
    auto rows = db_.query(
        "SELECT id, user_id, created_at, expires_at, status "
        "FROM sessions WHERE user_id = ? ORDER BY created_at DESC",
        {user_id}
    );

    std::vector<Session> sessions;
    sessions.reserve(rows.size());
    for (auto& row : rows) {
        Session s;
        s.id         = row.at("id");
        s.user_id    = row.at("user_id");
        s.created_at = iso_to_tp(row.at("created_at"));
        s.expires_at = iso_to_tp(row.at("expires_at"));
        std::string st = row.at("status");
        if (st == "revoked")       s.status = SessionStatus::REVOKED;
        else if (st == "expired")  s.status = SessionStatus::EXPIRED;
        else                       s.status = SessionStatus::ACTIVE;
        sessions.push_back(std::move(s));
    }
    return sessions;
}

bool SessionManager::delete_session(const std::string& session_id,
                                    const std::string& requesting_user_id) noexcept {
    // Ownership-enforced delete (user can only delete own sessions)
    return revoke_session(session_id, requesting_user_id);
}

std::string SessionManager::generate_session_id() const {
    return make_uuid();
}

} // namespace auth
