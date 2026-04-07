#pragma once
#include <string>
#include <filesystem>

namespace vega {

struct AppSettings {
    // Window
    int window_x = 100, window_y = 100;
    int window_w = 1920, window_h = 1080;
    bool maximized = true;

    // UI
    float ui_scale = 1.0f;
    bool dark_theme = true;
    std::string language = "en";  // "en" or "zh_tw"

    // Performance
    bool use_gpu = true;
    int preview_quality = 2;  // 0=fast, 1=medium, 2=full

    // Paths
    std::string last_open_dir;
    std::string last_export_dir;
    std::string catalog_path;
    std::vector<std::string> library_folders;

    // Load/Save
    static AppSettings load();
    void save() const;
    static std::filesystem::path settingsPath();
};

} // namespace vega
