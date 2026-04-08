#pragma once
#include <filesystem>
#include <string>
#include <optional>

struct sqlite3;

namespace vega {

// Simple key-value SQLite store for UI state and app settings.
// Replaces both settings.json and imgui .ini file.
class UIStateDB {
public:
    bool open(const std::filesystem::path& db_path);
    void close();
    bool isOpen() const { return db_ != nullptr; }

    // Key-value operations
    void set(const std::string& key, const std::string& value);
    void setInt(const std::string& key, int value);
    void setFloat(const std::string& key, float value);
    void setBool(const std::string& key, bool value);

    std::optional<std::string> get(const std::string& key);
    int getInt(const std::string& key, int fallback = 0);
    float getFloat(const std::string& key, float fallback = 0.0f);
    bool getBool(const std::string& key, bool fallback = false);

    // Bulk: store/retrieve ImGui layout as a single text blob
    void setImGuiLayout(const std::string& ini_data);
    std::optional<std::string> getImGuiLayout();

private:
    sqlite3* db_ = nullptr;
    bool exec(const char* sql);
};

} // namespace vega
