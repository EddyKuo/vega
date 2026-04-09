#include "core/i18n.h"
#include "core/Logger.h"

#include <sqlite3.h>
#include <filesystem>

namespace vega {

I18n& I18n::instance()
{
    static I18n inst;
    return inst;
}

I18n::I18n() = default;

I18n::~I18n()
{
    closeDatabase();
}

bool I18n::openDatabase(const std::filesystem::path& db_path)
{
    closeDatabase();

    if (!std::filesystem::exists(db_path)) {
        VEGA_LOG_ERROR("I18n: language database not found: {}", db_path.string());
        return false;
    }

    int rc = sqlite3_open_v2(db_path.string().c_str(), &db_,
        SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX, nullptr);
    if (rc != SQLITE_OK) {
        VEGA_LOG_ERROR("I18n: failed to open {}: {}", db_path.string(), sqlite3_errmsg(db_));
        sqlite3_close(db_);
        db_ = nullptr;
        return false;
    }

    VEGA_LOG_INFO("I18n: opened language database: {}", db_path.string());

    // Load current language
    loadStrings(lang_code_);
    return true;
}

void I18n::closeDatabase()
{
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

void I18n::setLanguage(const std::string& lang_code)
{
    if (lang_code == lang_code_ && !strings_.empty()) return;

    lang_code_ = lang_code;

    if (lang_code == "zh_tw")
        lang_enum_ = Lang::ZH_TW;
    else
        lang_enum_ = Lang::EN;

    loadStrings(lang_code_);
}

void I18n::setLanguage(Lang lang)
{
    lang_enum_ = lang;
    switch (lang) {
        case Lang::ZH_TW: setLanguage(std::string("zh_tw")); break;
        default:           setLanguage(std::string("en")); break;
    }
}

const char* I18n::get(const char* key) const
{
    auto it = strings_.find(key);
    if (it != strings_.end())
        return it->second.c_str();
    return key; // fallback to key itself
}

std::vector<LanguageInfo> I18n::availableLanguages() const
{
    std::vector<LanguageInfo> result;
    if (!db_) return result;

    const char* sql = "SELECT code, name, native FROM languages ORDER BY code;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK)
        return result;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        LanguageInfo info;
        const char* code   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* name   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        const char* native = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        if (code) info.code = code;
        if (name) info.name = name;
        if (native) info.native = native;
        result.push_back(std::move(info));
    }

    sqlite3_finalize(stmt);
    return result;
}

void I18n::loadStrings(const std::string& lang_code)
{
    strings_.clear();

    if (!db_) {
        VEGA_LOG_WARN("I18n: no database loaded, translations unavailable");
        return;
    }

    const char* sql = "SELECT key, value FROM translations WHERE lang = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        VEGA_LOG_ERROR("I18n: failed to prepare query: {}", sqlite3_errmsg(db_));
        return;
    }

    sqlite3_bind_text(stmt, 1, lang_code.c_str(), -1, SQLITE_TRANSIENT);

    int count = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* key = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        const char* val = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        if (key && val) {
            strings_[key] = val;
            count++;
        }
    }

    sqlite3_finalize(stmt);
    VEGA_LOG_INFO("I18n: loaded {} strings for '{}'", count, lang_code);
}

} // namespace vega
