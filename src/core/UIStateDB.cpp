#define NOMINMAX
#include "core/UIStateDB.h"
#include "core/Logger.h"

#include <sqlite3.h>
#include <filesystem>

namespace vega {

bool UIStateDB::open(const std::filesystem::path& db_path)
{
    if (db_) close();

    auto parent = db_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
    }

    int rc = sqlite3_open(db_path.string().c_str(), &db_);
    if (rc != SQLITE_OK) {
        VEGA_LOG_ERROR("UIStateDB: failed to open {}: {}", db_path.string(), sqlite3_errmsg(db_));
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    exec("PRAGMA journal_mode=WAL;");

    exec(R"SQL(
        CREATE TABLE IF NOT EXISTS ui_state (
            key   TEXT PRIMARY KEY,
            value TEXT NOT NULL
        );
    )SQL");

    VEGA_LOG_INFO("UIStateDB: opened {}", db_path.string());
    return true;
}

void UIStateDB::close()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool UIStateDB::exec(const char* sql)
{
    if (!db_) return false;
    char* err = nullptr;
    int rc = sqlite3_exec(db_, sql, nullptr, nullptr, &err);
    if (rc != SQLITE_OK) {
        VEGA_LOG_ERROR("UIStateDB::exec: {}", err ? err : "unknown");
        sqlite3_free(err);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Key-value setters
// ---------------------------------------------------------------------------

void UIStateDB::set(const std::string& key, const std::string& value)
{
    if (!db_) return;
    const char* sql = "INSERT OR REPLACE INTO ui_state (key, value) VALUES (?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return;
    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
}

void UIStateDB::setInt(const std::string& key, int value)
{
    set(key, std::to_string(value));
}

void UIStateDB::setFloat(const std::string& key, float value)
{
    set(key, std::to_string(value));
}

void UIStateDB::setBool(const std::string& key, bool value)
{
    set(key, value ? "1" : "0");
}

// ---------------------------------------------------------------------------
// Key-value getters
// ---------------------------------------------------------------------------

std::optional<std::string> UIStateDB::get(const std::string& key)
{
    if (!db_) return std::nullopt;
    const char* sql = "SELECT value FROM ui_state WHERE key = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return std::nullopt;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    std::optional<std::string> result;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        if (text) result = std::string(text);
    }
    sqlite3_finalize(stmt);
    return result;
}

int UIStateDB::getInt(const std::string& key, int fallback)
{
    auto val = get(key);
    if (!val) return fallback;
    try { return std::stoi(*val); }
    catch (...) { return fallback; }
}

float UIStateDB::getFloat(const std::string& key, float fallback)
{
    auto val = get(key);
    if (!val) return fallback;
    try { return std::stof(*val); }
    catch (...) { return fallback; }
}

bool UIStateDB::getBool(const std::string& key, bool fallback)
{
    auto val = get(key);
    if (!val) return fallback;
    return *val == "1" || *val == "true";
}

// ---------------------------------------------------------------------------
// ImGui layout blob
// ---------------------------------------------------------------------------

void UIStateDB::setImGuiLayout(const std::string& ini_data)
{
    set("imgui_layout", ini_data);
}

std::optional<std::string> UIStateDB::getImGuiLayout()
{
    return get("imgui_layout");
}

} // namespace vega
