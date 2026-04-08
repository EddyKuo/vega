#define NOMINMAX
#include "core/Settings.h"
#include "core/Logger.h"

#include <windows.h>
#include <shlobj.h>
#include <fstream>
#include <nlohmann/json.hpp>

namespace vega {

std::filesystem::path AppSettings::settingsPath()
{
    // Use %APPDATA%/Vega/settings.json
    wchar_t* appdata_raw = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata_raw)))
    {
        std::filesystem::path dir = std::filesystem::path(appdata_raw) / "Vega";
        CoTaskMemFree(appdata_raw);
        return dir / "settings.json";
    }

    // Fallback: next to the executable
    return "settings.json";
}

AppSettings AppSettings::load()
{
    AppSettings settings;
    auto path = settingsPath();

    std::ifstream ifs(path);
    if (!ifs.is_open())
    {
        VEGA_LOG_INFO("No settings file found at {}, using defaults", path.string());
        return settings;
    }

    try
    {
        nlohmann::json j = nlohmann::json::parse(ifs);

        // Window
        if (j.contains("window_x"))      settings.window_x      = j["window_x"].get<int>();
        if (j.contains("window_y"))      settings.window_y      = j["window_y"].get<int>();
        if (j.contains("window_w"))      settings.window_w      = j["window_w"].get<int>();
        if (j.contains("window_h"))      settings.window_h      = j["window_h"].get<int>();
        if (j.contains("maximized"))     settings.maximized      = j["maximized"].get<bool>();

        // UI
        if (j.contains("ui_scale"))      settings.ui_scale       = j["ui_scale"].get<float>();
        if (j.contains("dark_theme"))    settings.dark_theme     = j["dark_theme"].get<bool>();
        if (j.contains("language"))      settings.language        = j["language"].get<std::string>();

        // Performance
        if (j.contains("use_gpu"))       settings.use_gpu        = j["use_gpu"].get<bool>();
        if (j.contains("preview_quality")) settings.preview_quality = j["preview_quality"].get<int>();

        // Paths
        if (j.contains("last_open_dir"))   settings.last_open_dir   = j["last_open_dir"].get<std::string>();
        if (j.contains("last_export_dir")) settings.last_export_dir = j["last_export_dir"].get<std::string>();
        if (j.contains("catalog_path"))    settings.catalog_path    = j["catalog_path"].get<std::string>();
        if (j.contains("library_folders")) {
            for (const auto& f : j["library_folders"])
                settings.library_folders.push_back(f.get<std::string>());
        }
        if (j.contains("selected_folder")) settings.selected_folder = j["selected_folder"].get<std::string>();

        VEGA_LOG_INFO("Settings loaded from {}", path.string());
    }
    catch (const nlohmann::json::exception& e)
    {
        VEGA_LOG_WARN("Failed to parse settings: {}, using defaults", e.what());
    }

    // Validate window geometry — reject garbage values
    if (settings.window_w < 200 || settings.window_h < 200 ||
        settings.window_w > 8192 || settings.window_h > 8192 ||
        settings.window_x < -4096 || settings.window_x > 8192 ||
        settings.window_y < -4096 || settings.window_y > 8192)
    {
        VEGA_LOG_WARN("Invalid window geometry (x={}, y={}, w={}, h={}), resetting to defaults",
            settings.window_x, settings.window_y, settings.window_w, settings.window_h);
        settings.window_x = 100;
        settings.window_y = 100;
        settings.window_w = 1920;
        settings.window_h = 1080;
        settings.maximized = true;
    }

    return settings;
}

void AppSettings::save() const
{
    auto path = settingsPath();

    // Ensure the directory exists
    auto dir = path.parent_path();
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec)
    {
        VEGA_LOG_ERROR("Failed to create settings directory {}: {}", dir.string(), ec.message());
        return;
    }

    nlohmann::json j;

    // Window
    j["window_x"]       = window_x;
    j["window_y"]       = window_y;
    j["window_w"]       = window_w;
    j["window_h"]       = window_h;
    j["maximized"]      = maximized;

    // UI
    j["ui_scale"]        = ui_scale;
    j["dark_theme"]      = dark_theme;
    j["language"]        = language;

    // Performance
    j["use_gpu"]         = use_gpu;
    j["preview_quality"] = preview_quality;

    // Paths
    j["last_open_dir"]   = last_open_dir;
    j["last_export_dir"] = last_export_dir;
    j["catalog_path"]    = catalog_path;
    j["library_folders"] = nlohmann::json::array();
    for (const auto& f : library_folders)
        j["library_folders"].push_back(f);
    j["selected_folder"] = selected_folder;

    std::ofstream ofs(path);
    if (!ofs.is_open())
    {
        VEGA_LOG_ERROR("Failed to write settings to {}", path.string());
        return;
    }

    ofs << j.dump(4) << std::endl;
    VEGA_LOG_DEBUG("Settings saved to {}", path.string());
}

} // namespace vega
