// src/main.cpp — Vega RAW Photo Editor
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include "core/Logger.h"
#include "core/Timer.h"
#include "core/CrashHandler.h"
#include "core/Settings.h"
#include "core/UIStateDB.h"
#include "core/WindowsIntegration.h"
#include "gpu/D3D11Context.h"
#include "raw/RawDecoder.h"
#include "raw/ExifReader.h"
#include "pipeline/Pipeline.h"
#include "pipeline/EditRecipe.h"
#include "pipeline/EditHistory.h"
#include "ui/ImageViewport.h"
#include "ui/DevelopPanel.h"
#include "ui/HistogramView.h"
#include "ui/BeforeAfter.h"
#include "ui/ExportDialog.h"
#include "ui/Toolbar.h"
#include "ui/StatusBar.h"
#include "ui/GridView.h"
#include "ui/FolderPanel.h"
#include "catalog/Database.h"
#include "catalog/ThumbnailCache.h"
#include "catalog/ImportManager.h"
#include "core/i18n.h"
#include "pipeline/GPUPipeline.h"
#include "pipeline/AutoTone.h"

#include <shlobj.h>

using Microsoft::WRL::ComPtr;

// ── App Mode ──
enum class AppMode { Library, Develop };

// ── Globals ──
static AppMode g_app_mode = AppMode::Library;
static vega::D3D11Context g_ctx;
static vega::ImageViewport g_viewport;
static vega::DevelopPanel g_develop_panel;
static vega::HistogramView g_histogram;
static vega::Pipeline g_pipeline;
static vega::GPUPipeline g_gpu_pipeline;
static bool g_use_gpu = false;  // true once GPU pipeline init succeeds
static vega::EditRecipe g_recipe;
static vega::EditHistory g_history;
static vega::BeforeAfter g_before_after;
static vega::ExportDialog g_export_dialog;
static vega::Toolbar g_toolbar;
static vega::StatusBar g_status_bar;
static vega::AppSettings g_settings;
static vega::UIStateDB g_ui_db;

// Library mode
static vega::GridView g_grid_view;
static vega::FolderPanel g_folder_panel;
static vega::Database g_database;
static vega::ThumbnailCache g_thumb_cache;

// Import state
static std::atomic<bool> g_import_running{false};
static vega::ImportManager::ImportProgress g_import_progress;
static std::mutex g_import_mutex;

// ---------------------------------------------------------------------------
// Load/save app settings from UIStateDB
// ---------------------------------------------------------------------------
static void loadSettingsFromDB()
{
    g_settings.window_x   = g_ui_db.getInt("window_x", 100);
    g_settings.window_y   = g_ui_db.getInt("window_y", 100);
    g_settings.window_w   = g_ui_db.getInt("window_w", 1920);
    g_settings.window_h   = g_ui_db.getInt("window_h", 1080);
    g_settings.maximized  = g_ui_db.getBool("maximized", true);
    g_settings.ui_scale   = g_ui_db.getFloat("ui_scale", 1.0f);
    g_settings.dark_theme = g_ui_db.getBool("dark_theme", true);
    g_settings.language   = g_ui_db.get("language").value_or("en");
    g_settings.use_gpu    = g_ui_db.getBool("use_gpu", true);
    g_settings.preview_quality = g_ui_db.getInt("preview_quality", 2);
    g_settings.last_open_dir   = g_ui_db.get("last_open_dir").value_or("");
    g_settings.last_export_dir = g_ui_db.get("last_export_dir").value_or("");
    g_settings.catalog_path    = g_ui_db.get("catalog_path").value_or("");
    g_settings.selected_folder = g_ui_db.get("selected_folder").value_or("");

    // Restore library folders list (stored as comma-separated or individual keys)
    g_settings.library_folders.clear();
    int folder_count = g_ui_db.getInt("library_folder_count", 0);
    for (int i = 0; i < folder_count; ++i) {
        auto val = g_ui_db.get("library_folder_" + std::to_string(i));
        if (val && !val->empty())
            g_settings.library_folders.push_back(*val);
    }

    // Validate window geometry
    if (g_settings.window_w < 200 || g_settings.window_h < 200 ||
        g_settings.window_w > 8192 || g_settings.window_h > 8192 ||
        g_settings.window_x < -4096 || g_settings.window_x > 8192 ||
        g_settings.window_y < -4096 || g_settings.window_y > 8192)
    {
        g_settings.window_x = 100; g_settings.window_y = 100;
        g_settings.window_w = 1920; g_settings.window_h = 1080;
        g_settings.maximized = true;
    }
}

static void saveSettingsToDB()
{
    g_ui_db.setInt("window_x", g_settings.window_x);
    g_ui_db.setInt("window_y", g_settings.window_y);
    g_ui_db.setInt("window_w", g_settings.window_w);
    g_ui_db.setInt("window_h", g_settings.window_h);
    g_ui_db.setBool("maximized", g_settings.maximized);
    g_ui_db.setFloat("ui_scale", g_settings.ui_scale);
    g_ui_db.setBool("dark_theme", g_settings.dark_theme);
    g_ui_db.set("language", g_settings.language);
    g_ui_db.setBool("use_gpu", g_settings.use_gpu);
    g_ui_db.setInt("preview_quality", g_settings.preview_quality);
    g_ui_db.set("last_open_dir", g_settings.last_open_dir);
    g_ui_db.set("last_export_dir", g_settings.last_export_dir);
    g_ui_db.set("catalog_path", g_settings.catalog_path);
    g_ui_db.set("selected_folder", g_settings.selected_folder);

    int new_count = static_cast<int>(g_settings.library_folders.size());
    int old_count = g_ui_db.getInt("library_folder_count", 0);
    g_ui_db.setInt("library_folder_count", new_count);
    for (int i = 0; i < new_count; ++i) {
        g_ui_db.set("library_folder_" + std::to_string(i), g_settings.library_folders[i]);
    }
    // Remove stale keys from previous saves
    for (int i = new_count; i < old_count; ++i) {
        g_ui_db.set("library_folder_" + std::to_string(i), "");
    }

    // Save ImGui layout (only if ImGui context exists)
    if (ImGui::GetCurrentContext()) {
        const char* ini = ImGui::SaveIniSettingsToMemory();
        if (ini) g_ui_db.setImGuiLayout(ini);
    }
}

// Window
static HWND g_hwnd = nullptr;

// Image state
static vega::RawImage g_raw_image;
static bool g_has_image = false;
static ComPtr<ID3D11ShaderResourceView> g_image_srv;
static ComPtr<ID3D11Texture2D> g_image_tex;
static ComPtr<ID3D11ShaderResourceView> g_before_srv;
static ComPtr<ID3D11Texture2D> g_before_tex;
static const std::vector<uint8_t>* g_rgba_ptr = nullptr;
static std::vector<uint8_t> g_before_rgba;
static std::filesystem::path g_current_path;
static bool g_show_before_after = false;
static bool g_first_frame = true;
static double g_last_pipeline_ms = 0.0;
static bool g_needs_full_res = false;
static bool g_is_preview = false;
static uint32_t g_display_w = 0, g_display_h = 0;

// Background pipeline thread
#include <thread>
#include <mutex>
#include <atomic>
static std::mutex g_bg_mutex;
static std::vector<uint8_t> g_bg_result;         // background thread writes here
static uint32_t g_bg_w = 0, g_bg_h = 0;
static std::atomic<bool> g_bg_ready{false};       // set by bg thread when done
static std::atomic<bool> g_bg_running{false};     // bg thread is active
static std::atomic<bool> g_bg_cancel{false};      // request bg thread to stop
static vega::Pipeline g_bg_pipeline;              // separate pipeline for bg thread
static double g_bg_pipeline_ms = 0.0;
static std::thread g_bg_thread;                   // joinable background thread

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_ctx.device() && wParam != SIZE_MINIMIZED)
            g_ctx.resize(LOWORD(lParam), HIWORD(lParam));
        return 0;
    case WM_DROPFILES:
    {
        HDROP hDrop = reinterpret_cast<HDROP>(wParam);
        wchar_t path[MAX_PATH];
        if (DragQueryFileW(hDrop, 0, path, MAX_PATH))
        {
            g_current_path = std::filesystem::path(path);
            // Will be handled next frame via a flag
        }
        DragFinish(hDrop);
        return 0;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// ── GPU Upload ──
static void uploadToGPU(const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h,
                        ComPtr<ID3D11Texture2D>& tex, ComPtr<ID3D11ShaderResourceView>& srv)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w; desc.Height = h; desc.MipLevels = 1; desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = {1, 0};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = rgba.data();
    init.SysMemPitch = w * 4;
    tex.Reset(); srv.Reset();
    HRESULT hr = g_ctx.device()->CreateTexture2D(&desc, &init, &tex);
    if (SUCCEEDED(hr))
        g_ctx.device()->CreateShaderResourceView(tex.Get(), nullptr, &srv);
}

// Pending recipe for bg thread to pick up when current run finishes
static vega::EditRecipe g_bg_pending_recipe;
static std::atomic<bool> g_bg_has_pending{false};

// Launch full-res processing on background thread (CPU)
static void launchBackgroundProcess()
{
    if (g_bg_running) {
        // Thread busy — queue this recipe for when it finishes
        g_bg_pending_recipe = g_recipe;
        g_bg_has_pending = true;
        g_bg_cancel = true;  // cancel current work so it picks up pending faster
        return;
    }

    // Join previous thread before launching a new one
    if (g_bg_thread.joinable()) g_bg_thread.join();

    g_bg_cancel = false;
    g_bg_running = true;
    g_bg_ready = false;
    g_bg_has_pending = false;

    // Copy raw image data to avoid data race with main thread
    vega::EditRecipe recipe_copy = g_recipe;
    vega::RawImage raw_copy = g_raw_image;
    g_bg_thread = std::thread([recipe_copy, raw_copy = std::move(raw_copy)]() {
        vega::Timer timer;
        const auto& rgba = g_bg_pipeline.process(raw_copy, recipe_copy);
        if (g_bg_cancel) {
            g_bg_running = false;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(g_bg_mutex);
            g_bg_result.assign(rgba.begin(), rgba.end());
            g_bg_w = raw_copy.width;
            g_bg_h = raw_copy.height;
            g_bg_pipeline_ms = timer.elapsed_ms();
        }
        g_bg_ready = true;
        g_bg_running = false;
    });
}

// Call every frame: if bg finished and there's a pending recipe, relaunch
static void checkPendingBackground()
{
    if (!g_bg_running && g_bg_has_pending) {
        g_bg_has_pending = false;
        g_recipe = g_bg_pending_recipe;  // already set
        launchBackgroundProcess();
    }
}

// CPU preview on UI thread (1/8 res, reduced denoise/sharpen radius)
static void reprocessPreview()
{
    if (!g_has_image) return;
    vega::Timer timer;

    // At 1/8 res (740x493), all nodes are fast. Keep denoise/sharpen
    // so the user sees their effect immediately.
    uint32_t pw, ph;
    const auto& rgba = g_pipeline.processPreview(g_raw_image, g_recipe, 8, pw, ph);
    g_rgba_ptr = &rgba;
    g_last_pipeline_ms = timer.elapsed_ms();
    g_display_w = pw;
    g_display_h = ph;
    g_is_preview = true;
    uploadToGPU(rgba, pw, ph, g_image_tex, g_image_srv);
    g_histogram.compute(rgba.data(), pw, ph);
}

// Main entry: try GPU first, fallback to CPU preview + background
static void reprocessPipeline()
{
    if (!g_has_image) return;

    if (g_use_gpu) {
        vega::Timer timer;
        ID3D11ShaderResourceView* srv = g_gpu_pipeline.process(g_raw_image, g_recipe);
        double ms = timer.elapsed_ms();
        if (srv) {
            g_image_srv.Reset();
            g_image_tex.Reset();
            srv->AddRef();
            g_image_srv.Attach(srv);
            g_display_w = g_gpu_pipeline.outputWidth();
            g_display_h = g_gpu_pipeline.outputHeight();
            g_is_preview = false;
            g_last_pipeline_ms = ms;
            // Histogram from CPU preview (small, fast)
            uint32_t pw, ph;
            const auto& pr = g_pipeline.processPreview(g_raw_image, g_recipe, 8, pw, ph);
            g_rgba_ptr = &pr;
            g_histogram.compute(pr.data(), pw, ph);
            return;
        }
        // GPU failed — disable and fallback
        VEGA_LOG_WARN("GPU pipeline returned null, disabling GPU");
        g_use_gpu = false;
    }

    // CPU path: instant preview + deferred full-res
    reprocessPreview();
    g_needs_full_res = true;
    launchBackgroundProcess();
}

// Check if background CPU result is ready
static void pollBackgroundResult()
{
    if (!g_bg_ready) return;
    std::lock_guard<std::mutex> lock(g_bg_mutex);
    g_last_pipeline_ms = g_bg_pipeline_ms;
    g_display_w = g_bg_w;
    g_display_h = g_bg_h;
    g_is_preview = false;
    g_needs_full_res = false;
    g_rgba_ptr = &g_bg_result;
    uploadToGPU(g_bg_result, g_bg_w, g_bg_h, g_image_tex, g_image_srv);
    g_histogram.compute(g_bg_result.data(), g_bg_w, g_bg_h);
    g_bg_ready = false;
}

static bool g_before_dirty = true;  // needs regeneration
static std::thread g_before_thread;
static std::vector<uint8_t> g_before_pending;
static std::atomic<bool> g_before_ready{false};

// Generate "before" image on a background thread (lazy, only when needed)
static void generateBeforeImageAsync()
{
    if (!g_has_image || !g_before_dirty) return;
    if (g_before_thread.joinable()) return;  // already running

    g_before_dirty = false;
    g_before_ready = false;

    // Copy raw image to avoid data race with main thread
    vega::RawImage raw_copy = g_raw_image;
    g_before_thread = std::thread([raw_copy = std::move(raw_copy)]() {
        vega::Pipeline pipe;
        auto rgba = pipe.process(raw_copy, vega::EditRecipe{});
        g_before_pending = std::move(rgba);
        g_before_ready = true;
    });
}

static void pollBeforeImage()
{
    if (!g_before_ready) return;
    if (g_before_thread.joinable()) g_before_thread.join();

    g_before_rgba = std::move(g_before_pending);
    uploadToGPU(g_before_rgba, g_raw_image.width, g_raw_image.height, g_before_tex, g_before_srv);
    g_before_ready = false;
}

// Common decode + setup logic for opening a RAW file
static bool setupRawImage(const std::filesystem::path& path)
{
    VEGA_LOG_INFO("Opening: {}", path.string());
    vega::Timer timer;

    auto result = vega::RawDecoder::decode(path);
    if (!result) {
        g_status_bar.filename = "Error: Failed to decode RAW file";
        return false;
    }

    g_current_path = path;
    g_raw_image = std::move(result.value());
    VEGA_LOG_INFO("Decoded {}x{} in {:.1f}ms", g_raw_image.width, g_raw_image.height, timer.elapsed_ms());

    vega::ExifReader::enrichMetadata(path, g_raw_image.metadata);
    VEGA_LOG_INFO("EXIF: {} {} | ISO {} | {:.4f}s | f/{:.1f} | {}mm",
        g_raw_image.metadata.camera_make, g_raw_image.metadata.camera_model,
        g_raw_image.metadata.iso_speed, g_raw_image.metadata.shutter_speed,
        g_raw_image.metadata.aperture, g_raw_image.metadata.focal_length_mm);

    auto saved = vega::loadRecipe(path);
    if (saved) VEGA_LOG_INFO("Loaded .vgr sidecar recipe");
    g_recipe = saved ? *saved : vega::EditRecipe{};
    g_history.clear();
    g_has_image = true;
    g_develop_panel.setAsShotWB(5500.0f, 0.0f);
    g_develop_panel.setImageDimensions(g_raw_image.width, g_raw_image.height);

    if (g_use_gpu)
        g_gpu_pipeline.uploadRawData(g_raw_image);

    reprocessPipeline();
    g_before_dirty = true;

    ImVec2 vp_size = ImGui::GetMainViewport()->WorkSize;
    g_viewport.fitToWindow(ImVec2(vp_size.x * 0.65f, vp_size.y * 0.8f),
                           g_raw_image.width, g_raw_image.height);

    auto& m = g_raw_image.metadata;
    g_status_bar.filename = path.filename().string();
    g_status_bar.camera_info = m.camera_make + " " + m.camera_model;
    g_status_bar.resolution = std::to_string(g_raw_image.width) + "x" + std::to_string(g_raw_image.height);

    SetWindowTextW(g_hwnd, (L"Vega - " + path.filename().wstring()).c_str());
    g_app_mode = AppMode::Develop;
    return true;
}

static void openRawFile()
{
    wchar_t filename[MAX_PATH] = {};
    OPENFILENAMEW ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"RAW Files\0*.cr3;*.cr2;*.arw;*.nef;*.raf;*.dng;*.orf;*.rw2;*.pef\0"
                      L"All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Open RAW File";

    if (!g_settings.last_open_dir.empty())
    {
        std::wstring wdir(g_settings.last_open_dir.begin(), g_settings.last_open_dir.end());
        ofn.lpstrInitialDir = wdir.c_str();
    }

    if (!GetOpenFileNameW(&ofn))
        return;

    auto path = std::filesystem::path(filename);
    g_settings.last_open_dir = path.parent_path().string();
    setupRawImage(path);
}

static void loadRawAndDevelop(const std::filesystem::path& path)
{
    setupRawImage(path);
}

// Win32 folder picker using IFileDialog (Vista+)
static std::filesystem::path pickFolder()
{
    std::filesystem::path result;
    IFileDialog* pfd = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_PPV_ARGS(&pfd));
    if (FAILED(hr)) return result;

    DWORD options = 0;
    pfd->GetOptions(&options);
    pfd->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
    pfd->SetTitle(L"Select RAW Folder");

    hr = pfd->Show(g_hwnd);
    if (SUCCEEDED(hr)) {
        IShellItem* psi = nullptr;
        hr = pfd->GetResult(&psi);
        if (SUCCEEDED(hr)) {
            PWSTR path_str = nullptr;
            hr = psi->GetDisplayName(SIGDN_FILESYSPATH, &path_str);
            if (SUCCEEDED(hr) && path_str) {
                result = std::filesystem::path(path_str);
                CoTaskMemFree(path_str);
            }
            psi->Release();
        }
    }
    pfd->Release();
    return result;
}

// Background import state — results posted back to main thread via polling
static std::thread g_import_thread;
struct ImportResult {
    std::filesystem::path folder_path;
    int folder_index = -1;
    int64_t db_count = 0;
    bool done = false;
};
static ImportResult g_import_result;

// Import a folder in background thread
static void importFolderAsync(const std::filesystem::path& folder_path, int folder_index)
{
    if (g_import_running) return;

    // Wait for previous thread to finish
    if (g_import_thread.joinable()) g_import_thread.join();

    g_import_running = true;
    g_folder_panel.setImporting(folder_index, true);

    {
        std::lock_guard<std::mutex> lock(g_import_mutex);
        g_import_result = {};
        g_import_result.folder_index = folder_index;
        g_import_result.folder_path = folder_path;
    }

    std::filesystem::path folder_copy = folder_path;
    g_import_thread = std::thread([folder_copy, folder_index]() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);

        auto files = vega::ImportManager::scanDirectory(folder_copy);

        vega::ImportManager mgr;
        mgr.import(files, g_database, g_thumb_cache,
            [&](const vega::ImportManager::ImportProgress& p) {
                std::lock_guard<std::mutex> lock(g_import_mutex);
                g_import_progress = p;
            });

        // Post result for main thread to pick up (don't touch UI from here)
        int64_t db_count = g_database.countByFolder(folder_copy.string());
        {
            std::lock_guard<std::mutex> lock(g_import_mutex);
            g_import_result.db_count = db_count;
            g_import_result.done = true;
        }

        VEGA_LOG_INFO("Import complete for folder: {}", folder_copy.string());
        CoUninitialize();
    });
}

// Call from main thread each frame to apply import results safely
static void pollImportResult()
{
    if (!g_import_running) return;

    std::lock_guard<std::mutex> lock(g_import_mutex);
    if (!g_import_result.done) return;

    int idx = g_import_result.folder_index;
    g_folder_panel.updateRawCount(idx, static_cast<int>(g_import_result.db_count));
    g_folder_panel.setImporting(idx, false);
    g_grid_view.refresh();
    g_import_running = false;
}

// Add a folder: store in settings, add to panel, start import
static void addLibraryFolder()
{
    auto folder = pickFolder();
    if (folder.empty()) return;

    // Add to settings
    std::string folder_str = folder.string();
    auto& folders = g_settings.library_folders;
    if (std::find(folders.begin(), folders.end(), folder_str) == folders.end()) {
        folders.push_back(folder_str);
    }

    // Add to panel
    g_folder_panel.addFolder(folder);
    int idx = static_cast<int>(g_folder_panel.folders().size()) - 1;

    // Start background import
    importFolderAsync(folder, idx);
}

// Remove a folder from the panel and settings
static void removeLibraryFolder(int index)
{
    if (index < 0 || index >= static_cast<int>(g_folder_panel.folders().size())) return;

    auto path_str = g_folder_panel.folders()[index].path.string();
    g_folder_panel.removeFolder(index);

    // Remove from settings
    auto& folders = g_settings.library_folders;
    folders.erase(std::remove(folders.begin(), folders.end(), path_str), folders.end());

    g_grid_view.refresh();
}

// ── Vega dark theme ──
static void applyVegaTheme()
{
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding    = 4.0f;
    s.FrameRounding     = 3.0f;
    s.GrabRounding      = 3.0f;
    s.ScrollbarRounding = 6.0f;
    s.TabRounding       = 4.0f;
    s.WindowPadding     = ImVec2(8, 8);
    s.FramePadding      = ImVec2(6, 4);
    s.ItemSpacing       = ImVec2(8, 5);
    s.ScrollbarSize     = 14.0f;
    s.GrabMinSize       = 12.0f;
    s.WindowBorderSize  = 1.0f;
    s.FrameBorderSize   = 0.0f;
    s.TabBorderSize     = 0.0f;

    ImVec4* c = s.Colors;
    // Background
    c[ImGuiCol_WindowBg]        = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_ChildBg]         = ImVec4(0.12f, 0.12f, 0.14f, 1.00f);
    c[ImGuiCol_PopupBg]         = ImVec4(0.14f, 0.14f, 0.16f, 0.96f);
    // Borders
    c[ImGuiCol_Border]          = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    // Title
    c[ImGuiCol_TitleBg]         = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_TitleBgActive]   = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_TitleBgCollapsed]= ImVec4(0.10f, 0.10f, 0.12f, 0.50f);
    // Tabs
    c[ImGuiCol_Tab]             = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
    c[ImGuiCol_TabSelected]     = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    c[ImGuiCol_TabHovered]      = ImVec4(0.28f, 0.28f, 0.34f, 1.00f);
    // Frames (sliders, inputs)
    c[ImGuiCol_FrameBg]         = ImVec4(0.18f, 0.18f, 0.21f, 1.00f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.24f, 0.24f, 0.28f, 1.00f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
    // Buttons
    c[ImGuiCol_Button]          = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.30f, 0.30f, 0.36f, 1.00f);
    c[ImGuiCol_ButtonActive]    = ImVec4(0.38f, 0.38f, 0.44f, 1.00f);
    // Headers
    c[ImGuiCol_Header]          = ImVec4(0.20f, 0.20f, 0.24f, 1.00f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.28f, 0.28f, 0.34f, 1.00f);
    c[ImGuiCol_HeaderActive]    = ImVec4(0.34f, 0.34f, 0.40f, 1.00f);
    // Accent (sliders, checkboxes)
    c[ImGuiCol_SliderGrab]      = ImVec4(0.45f, 0.55f, 0.85f, 1.00f);
    c[ImGuiCol_SliderGrabActive]= ImVec4(0.55f, 0.65f, 0.95f, 1.00f);
    c[ImGuiCol_CheckMark]       = ImVec4(0.45f, 0.55f, 0.85f, 1.00f);
    // Scrollbar
    c[ImGuiCol_ScrollbarBg]     = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0.28f, 0.28f, 0.32f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.35f, 0.35f, 0.40f, 1.00f);
    c[ImGuiCol_ScrollbarGrabActive]  = ImVec4(0.42f, 0.42f, 0.48f, 1.00f);
    // Separator
    c[ImGuiCol_Separator]       = ImVec4(0.22f, 0.22f, 0.25f, 1.00f);
    // Resize grip
    c[ImGuiCol_ResizeGrip]      = ImVec4(0.30f, 0.30f, 0.36f, 0.25f);
    c[ImGuiCol_ResizeGripHovered]= ImVec4(0.45f, 0.55f, 0.85f, 0.67f);
    c[ImGuiCol_ResizeGripActive] = ImVec4(0.45f, 0.55f, 0.85f, 0.95f);
    // Docking
    c[ImGuiCol_DockingPreview]  = ImVec4(0.45f, 0.55f, 0.85f, 0.70f);
    c[ImGuiCol_DockingEmptyBg]  = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    // Text
    c[ImGuiCol_Text]            = ImVec4(0.88f, 0.88f, 0.90f, 1.00f);
    c[ImGuiCol_TextDisabled]    = ImVec4(0.50f, 0.50f, 0.52f, 1.00f);
}

// ── Build initial dock layout ──
static void buildDefaultLayout(ImGuiID dockspace_id)
{
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float tb_h = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
    float sb_h = ImGui::GetFrameHeight();
    ImGui::DockBuilderSetNodeSize(dockspace_id,
        ImVec2(vp->WorkSize.x, vp->WorkSize.y - tb_h - sb_h));

    // Split: left 75% (viewport area) | right 25% (develop panel)
    ImGuiID dock_right;
    ImGuiID dock_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.75f, nullptr, &dock_right);

    // Split left: folder panel 15% | main area 85%
    ImGuiID dock_main;
    ImGuiID dock_folders = ImGui::DockBuilderSplitNode(dock_left, ImGuiDir_Left, 0.18f, nullptr, &dock_main);

    // Split right: top 70% (develop) | bottom 30% (histogram)
    ImGuiID dock_right_bottom;
    ImGuiID dock_right_top = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.70f, nullptr, &dock_right_bottom);

    // Dock windows
    char buf[128];
    snprintf(buf, sizeof(buf), "%s###folders", vega::tr(vega::S::PANEL_FOLDERS));
    ImGui::DockBuilderDockWindow(buf, dock_folders);
    snprintf(buf, sizeof(buf), "%s###viewport", vega::tr(vega::S::PANEL_VIEWPORT));
    ImGui::DockBuilderDockWindow(buf, dock_main);
    snprintf(buf, sizeof(buf), "%s###develop", vega::tr(vega::S::PANEL_DEVELOP));
    ImGui::DockBuilderDockWindow(buf, dock_right_top);
    snprintf(buf, sizeof(buf), "%s###histogram", vega::tr(vega::S::PANEL_HISTOGRAM));
    ImGui::DockBuilderDockWindow(buf, dock_right_bottom);

    ImGui::DockBuilderFinish(dockspace_id);
}

// ── Menu bar ──
static void renderMenuBar()
{
    using namespace vega;
    if (ImGui::BeginMainMenuBar())
    {
        if (ImGui::BeginMenu(tr(S::MENU_FILE)))
        {
            if (ImGui::MenuItem(tr(S::MENU_OPEN), "Ctrl+O"))
                openRawFile();
            if (ImGui::MenuItem(tr(S::FOLDER_ADD), "Ctrl+Shift+I"))
                addLibraryFolder();
            if (ImGui::MenuItem(tr(S::MENU_SAVE_RECIPE), "Ctrl+S", false, g_has_image))
                saveRecipe(g_current_path, g_recipe);
            ImGui::Separator();
            if (ImGui::MenuItem(tr(S::MENU_EXPORT), "Ctrl+Shift+E", false, g_has_image))
                g_export_dialog.open(g_current_path);
            ImGui::Separator();
            if (ImGui::MenuItem(tr(S::MENU_EXIT), "Alt+F4"))
                PostQuitMessage(0);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr(S::MENU_EDIT)))
        {
            if (ImGui::MenuItem(tr(S::MENU_UNDO), "Ctrl+Z", false, g_history.canUndo()))
            {
                g_recipe = g_history.undo();
                reprocessPipeline();
            }
            if (ImGui::MenuItem(tr(S::MENU_REDO), "Ctrl+Y", false, g_history.canRedo()))
            {
                g_recipe = g_history.redo();
                reprocessPipeline();
            }
            ImGui::Separator();
            if (ImGui::MenuItem(tr(S::MENU_RESET_ALL), "Ctrl+Shift+R", false, g_has_image))
            {
                EditCommand cmd;
                cmd.description = "Reset All";
                cmd.before = g_recipe;
                cmd.after = EditRecipe{};
                cmd.affected_stage = PipelineStage::All;
                g_history.push(cmd);
                g_recipe = cmd.after;
                reprocessPipeline();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr(S::MENU_VIEW)))
        {
            if (ImGui::MenuItem(tr(S::MENU_FIT), "F"))
            {
                ImVec2 avail = ImGui::GetMainViewport()->WorkSize;
                g_viewport.fitToWindow(avail, g_raw_image.width, g_raw_image.height);
            }
            if (ImGui::MenuItem("Zoom 100%", "1"))
                ;
            if (ImGui::MenuItem("Zoom 200%", "2"))
                ;
            ImGui::Separator();
            if (ImGui::MenuItem(tr(S::MENU_BEFORE_AFTER), "B", g_show_before_after)) {
                g_show_before_after = !g_show_before_after;
                if (g_show_before_after && g_before_dirty) generateBeforeImageAsync();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr(S::MENU_LANGUAGE)))
        {
            auto langs = I18n::instance().availableLanguages();
            for (const auto& lang : langs) {
                bool selected = (I18n::instance().languageCode() == lang.code);
                if (ImGui::MenuItem(lang.native.c_str(), nullptr, selected)) {
                    I18n::instance().setLanguage(lang.code);
                    g_settings.language = lang.code;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    vega::installCrashHandler();
    vega::WindowsIntegration::enableHighDPI();
    vega::Logger::init();
    VEGA_LOG_INFO("=== Vega v0.1.0 starting ===");
    VEGA_LOG_INFO("Log file: vega.log");

    // Open UI state database (next to exe)
    {
        wchar_t exe_buf[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe_buf, MAX_PATH);
        auto exe_dir = std::filesystem::path(exe_buf).parent_path();
        g_ui_db.open(exe_dir / "ui_state.db");
    }

    // First run: migrate from settings.json if it exists
    if (!g_ui_db.get("language").has_value()) {
        auto old_settings = vega::AppSettings::load();
        g_settings = old_settings;
        // Write all settings into the new DB
        if (g_ui_db.isOpen()) {
            saveSettingsToDB();
            VEGA_LOG_INFO("Migrated settings.json to ui_state.db");
        }
    } else {
        loadSettingsFromDB();
    }

    VEGA_LOG_INFO("Settings: maximized={} gpu={} preview_quality={}",
        g_settings.maximized, g_settings.use_gpu, g_settings.preview_quality);

    // Window
    WNDCLASSEXW wc = {};
    wc.cbSize      = sizeof(WNDCLASSEXW);
    wc.style       = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance   = hInst;
    wc.lpszClassName = L"VegaEditor";
    RegisterClassExW(&wc);

    g_hwnd = CreateWindowW(wc.lpszClassName, L"Vega - RAW Photo Editor",
        WS_OVERLAPPEDWINDOW,
        g_settings.window_x, g_settings.window_y,
        g_settings.window_w, g_settings.window_h,
        nullptr, nullptr, hInst, nullptr);

    if (!g_ctx.initialize(g_hwnd, g_settings.window_w, g_settings.window_h))
    {
        VEGA_LOG_ERROR("Failed to initialize D3D11");
        return 1;
    }

    vega::WindowsIntegration::setDarkTitleBar(g_hwnd);
    vega::WindowsIntegration::enableDragDrop(g_hwnd);

    if (g_settings.maximized)
        ShowWindow(g_hwnd, SW_SHOWMAXIMIZED);
    else
        ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = nullptr;  // we manage layout via UIStateDB

    applyVegaTheme();

    // Restore ImGui layout from DB (if previously saved)
    auto saved_layout = g_ui_db.getImGuiLayout();
    bool has_saved_layout = saved_layout.has_value() && !saved_layout->empty();
    if (has_saved_layout) {
        ImGui::LoadIniSettingsFromMemory(saved_layout->c_str(), saved_layout->size());
    }

    ImGui_ImplWin32_Init(g_hwnd);
    ImGui_ImplDX11_Init(g_ctx.device(), g_ctx.context());

    // GPU pipeline
    if (g_settings.use_gpu && g_gpu_pipeline.initialize(g_ctx)) {
        g_use_gpu = true;
        VEGA_LOG_INFO("GPU pipeline enabled");
    } else {
        g_use_gpu = false;
        VEGA_LOG_INFO("GPU pipeline unavailable, using CPU path");
    }

    // Load fonts after backend init (ImGui requires backend before Build)
    {
        ImFontConfig latin_cfg;
        latin_cfg.SizePixels = 16.0f;
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f, &latin_cfg);

        ImFontConfig cjk_cfg;
        cjk_cfg.MergeMode = true;
        cjk_cfg.SizePixels = 16.0f;
        static const ImWchar cjk_ranges[] = {
            0x2000, 0x206F,   // General Punctuation
            0x3000, 0x30FF,   // CJK Symbols, Hiragana, Katakana
            0x31F0, 0x31FF,   // Katakana Phonetic Extensions
            0x4E00, 0x9FFF,   // CJK Unified Ideographs
            0xFF00, 0xFFEF,   // Halfwidth and Fullwidth Forms
            0,
        };
        io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\msjh.ttc", 16.0f, &cjk_cfg, cjk_ranges);
        VEGA_LOG_INFO("Fonts loaded: Segoe UI + Microsoft JhengHei (CJK)");
    }

    // Init i18n from language.db
    {
        wchar_t exe_buf[MAX_PATH]{};
        GetModuleFileNameW(nullptr, exe_buf, MAX_PATH);
        auto exe_dir = std::filesystem::path(exe_buf).parent_path();
        vega::I18n::instance().openDatabase(exe_dir / "language.db");
        vega::I18n::instance().setLanguage(g_settings.language);
    }

    // Toolbar callbacks
    vega::Toolbar::Callbacks tb_cb;
    tb_cb.on_open = []{ openRawFile(); };
    tb_cb.on_save_recipe = []{ if (g_has_image) vega::saveRecipe(g_current_path, g_recipe); };
    tb_cb.on_export = []{ if (g_has_image) g_export_dialog.open(g_current_path); };
    tb_cb.on_undo = []{ if (g_history.canUndo()) { g_recipe = g_history.undo(); reprocessPipeline(); } };
    tb_cb.on_redo = []{ if (g_history.canRedo()) { g_recipe = g_history.redo(); reprocessPipeline(); } };
    tb_cb.on_zoom_fit = []{
        ImVec2 avail = ImGui::GetMainViewport()->WorkSize;
        g_viewport.fitToWindow(avail, g_raw_image.width, g_raw_image.height);
    };
    tb_cb.on_zoom_100 = []{ /* handled by viewport F/1/2 keys */ };
    tb_cb.on_zoom_200 = []{};
    tb_cb.on_toggle_before_after = []{
        g_show_before_after = !g_show_before_after;
        if (g_show_before_after && g_before_dirty) generateBeforeImageAsync();
    };
    tb_cb.on_mode_grid = []{ g_app_mode = AppMode::Library; };
    tb_cb.on_mode_develop = []{ g_app_mode = AppMode::Develop; };
    g_toolbar.setCallbacks(tb_cb);

    // ── Initialize catalog database ──
    {
        std::string cat_path = g_settings.catalog_path;
        if (cat_path.empty()) {
            // Default catalog location: next to the executable
            wchar_t exe_buf[MAX_PATH]{};
            GetModuleFileNameW(nullptr, exe_buf, MAX_PATH);
            auto exe_dir = std::filesystem::path(exe_buf).parent_path();
            cat_path = (exe_dir / "catalog.db").string();
            g_settings.catalog_path = cat_path;
        }

        if (g_database.open(cat_path)) {
            VEGA_LOG_INFO("Catalog opened: {}", cat_path);
        } else {
            VEGA_LOG_ERROR("Failed to open catalog: {}", cat_path);
        }

        // Initialize thumbnail cache (DB-backed)
        g_thumb_cache.initialize(&g_database, g_ctx.device());
    }

    // ── Wire FolderPanel callbacks ──
    g_folder_panel.setOnAddFolder([]{ addLibraryFolder(); });
    g_folder_panel.setOnRemoveFolder([](int idx){ removeLibraryFolder(idx); });
    g_folder_panel.setOnFolderSelected([](const std::filesystem::path& path) {
        vega::Database::FilterCriteria filter;
        filter.folder_path = path.string();
        g_grid_view.setFilter(filter);
        g_settings.selected_folder = path.string();
    });
    g_folder_panel.setOnShowAll([]() {
        g_grid_view.setFilter(vega::Database::FilterCriteria{});
        g_settings.selected_folder.clear();
    });

    // ── Wire GridView double-click ──
    g_grid_view.setOnDoubleClick([](int64_t /*photo_id*/, const std::string& file_path) {
        loadRawAndDevelop(std::filesystem::path(file_path));
    });

    // ── Restore saved library folders ──
    for (const auto& folder_str : g_settings.library_folders) {
        std::filesystem::path folder_path(folder_str);
        if (std::filesystem::exists(folder_path)) {
            g_folder_panel.addFolder(folder_path);
            int idx = static_cast<int>(g_folder_panel.folders().size()) - 1;

            // Count existing photos in DB for this folder
            int64_t count = g_database.countByFolder(folder_str);
            if (count > 0) {
                g_folder_panel.updateRawCount(idx, static_cast<int>(count));
            } else {
                // No photos in DB yet — re-import in background
                importFolderAsync(folder_path, idx);
            }

            VEGA_LOG_INFO("Restored folder: {} ({} photos)", folder_str, count);
        }
    }

    // Restore last selected folder
    if (!g_settings.selected_folder.empty()) {
        g_folder_panel.selectByPath(std::filesystem::path(g_settings.selected_folder));
    }

    // WB Eyedropper callback: sample pixel from the demosaiced linear data
    g_viewport.setEyedropperCallback([](uint32_t x, uint32_t y) {
        if (!g_has_image) return;
        // Sample from the pipeline's demosaic cache (linear RGB before WB)
        // For simplicity, sample from the current RGBA output and reverse gamma
        if (!g_rgba_ptr || g_rgba_ptr->empty()) return;
        uint32_t w = g_raw_image.width;
        size_t idx = (static_cast<size_t>(y) * w + x) * 4;
        if (idx + 2 >= g_rgba_ptr->size()) return;

        float r = (*g_rgba_ptr)[idx + 0] / 255.0f;
        float g = (*g_rgba_ptr)[idx + 1] / 255.0f;
        float b = (*g_rgba_ptr)[idx + 2] / 255.0f;

        // The sampled pixel should be neutral grey — adjust WB to make it so.
        // Average the picked color and compute temperature shift.
        float avg = (r + g + b) / 3.0f;
        if (avg < 0.01f) return; // too dark to sample

        // Simple approach: adjust temperature based on blue/red ratio
        float rb_ratio = (b + 0.001f) / (r + 0.001f);
        // rb_ratio > 1 means too blue (need warmer), < 1 means too red (need cooler)
        // Map to temperature: multiply current temp by inverse ratio
        float new_temp = g_recipe.wb_temperature / std::sqrt(rb_ratio);
        new_temp = std::clamp(new_temp, 2000.0f, 12000.0f);

        vega::EditCommand cmd;
        cmd.description = "WB Eyedropper";
        cmd.before = g_recipe;
        g_recipe.wb_temperature = new_temp;
        cmd.after = g_recipe;
        cmd.affected_stage = vega::PipelineStage::WhiteBalance;
        g_history.push(cmd);

        VEGA_LOG_INFO("WB Eyedropper: pixel ({},{}) RGB=({:.2f},{:.2f},{:.2f}) -> temp={:.0f}K",
            x, y, r, g, b, new_temp);
    });

    // Main loop
    MSG msg = {};
    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // Flush decoded thumbnails from worker threads to GPU
        g_thumb_cache.flushPending();

        // Poll background pipeline result + relaunch if pending
        pollBackgroundResult();
        checkPendingBackground();
        pollImportResult();
        pollBeforeImage();

        // Menu bar
        renderMenuBar();

        // Toolbar (fixed at top, below menu bar)
        g_toolbar.render(g_has_image, g_history.canUndo(), g_history.canRedo(),
                         g_app_mode == AppMode::Develop);

        // Dockspace -- offset below toolbar and above status bar so they don't overlap
        ImGuiViewport* vp = ImGui::GetMainViewport();
        float toolbar_h = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
        float statusbar_h = ImGui::GetFrameHeight();
        float ds_h = vp->WorkSize.y - toolbar_h - statusbar_h;
        if (ds_h < 1.0f) ds_h = 1.0f;
        ImVec2 ds_pos(vp->WorkPos.x, vp->WorkPos.y + toolbar_h);
        ImVec2 ds_size(vp->WorkSize.x, ds_h);

        ImGui::SetNextWindowPos(ds_pos);
        ImGui::SetNextWindowSize(ds_size);
        ImGui::SetNextWindowViewport(vp->ID);
        ImGuiWindowFlags ds_flags =
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
            ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoSavedSettings;
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("##DockSpaceHost", nullptr, ds_flags);
        ImGui::PopStyleVar(3);
        ImGuiID dockspace_id = ImGui::GetID("VegaDockSpace");
        ImGui::DockSpace(dockspace_id);
        ImGui::End();

        // Build default layout on first frame if no saved layout exists
        if (g_first_frame)
        {
            g_first_frame = false;
            if (!has_saved_layout ||
                ImGui::DockBuilderGetNode(dockspace_id) == nullptr)
            {
                buildDefaultLayout(dockspace_id);
            }
        }

        // Keyboard shortcuts (global)
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
            openRawFile();
        if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_I))
            addLibraryFolder();
        // G key: switch to Library mode
        if (ImGui::IsKeyPressed(ImGuiKey_G, false) && !io.KeyCtrl && !io.KeyAlt)
            g_app_mode = AppMode::Library;
        // D key: switch to Develop mode
        if (ImGui::IsKeyPressed(ImGuiKey_D, false) && !io.KeyCtrl && !io.KeyAlt
            && g_app_mode == AppMode::Library)
            g_app_mode = AppMode::Develop;

        // Develop-mode shortcuts
        if (g_app_mode == AppMode::Develop) {
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && g_has_image)
            {
                VEGA_LOG_INFO("Saving recipe: {}", g_current_path.string());
                vega::saveRecipe(g_current_path, g_recipe);
            }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && g_history.canUndo())
            { g_recipe = g_history.undo(); reprocessPipeline(); }
            if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y) && g_history.canRedo())
            { g_recipe = g_history.redo(); reprocessPipeline(); }
            if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_E) && g_has_image)
                g_export_dialog.open(g_current_path);
            if (ImGui::IsKeyPressed(ImGuiKey_B, false) && !io.KeyCtrl && !io.KeyAlt) {
                g_show_before_after = !g_show_before_after;
                if (g_show_before_after && g_before_dirty)
                    generateBeforeImageAsync();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_M, false) && g_show_before_after)
                g_before_after.toggleMode();
            if (g_show_before_after)
                g_before_after.setShowBefore(ImGui::IsKeyDown(ImGuiKey_Backslash));
            if (ImGui::IsKeyPressed(ImGuiKey_W, false) && !io.KeyCtrl && g_has_image)
                g_viewport.activateEyedropper();
        }

        // ── Toolbar (always visible, rendered before dockspace to reserve space) ──
        // Already rendered above dockspace

        // ── Folder Panel (visible in both modes) ──
        {
        char folder_title[128];
        snprintf(folder_title, sizeof(folder_title), "%s###folders", vega::tr(vega::S::PANEL_FOLDERS));
        ImGui::Begin(folder_title);
        g_folder_panel.render();
        // Show import progress
        if (g_import_running) {
            std::lock_guard<std::mutex> lock(g_import_mutex);
            ImGui::Separator();
            char progress_buf[128];
            snprintf(progress_buf, sizeof(progress_buf), vega::tr(vega::S::IMPORT_PROGRESS),
                     g_import_progress.processed, g_import_progress.total_files);
            ImGui::TextDisabled("%s", progress_buf);
        }
        ImGui::End();
        }

        // ── Develop Panel (always rendered for docking) ──
        {
        char dev_title[128];
        snprintf(dev_title, sizeof(dev_title), "%s###develop", vega::tr(vega::S::PANEL_DEVELOP));
        ImGui::Begin(dev_title);
        if (g_app_mode == AppMode::Develop) {
            if (g_develop_panel.render(g_recipe, g_history))
                reprocessPipeline();

            if (g_develop_panel.auto_tone_requested) {
                g_develop_panel.auto_tone_requested = false;
                if (g_rgba_ptr && !g_rgba_ptr->empty()) {
                    auto at = vega::computeAutoTone(g_rgba_ptr->data(), g_display_w, g_display_h);
                    g_recipe.exposure   = at.exposure;
                    g_recipe.contrast   = at.contrast;
                    g_recipe.highlights = at.highlights;
                    g_recipe.shadows    = at.shadows;
                    g_recipe.whites     = at.whites;
                    g_recipe.blacks     = at.blacks;
                    reprocessPipeline();
                }
            }

            if (g_has_image && ImGui::CollapsingHeader(vega::tr("Metadata")))
            {
                auto& m = g_raw_image.metadata;
                ImGui::Text("Camera: %s %s", m.camera_make.c_str(), m.camera_model.c_str());
                ImGui::Text("Lens: %s", m.lens_model.c_str());
                ImGui::Text("ISO %u | %.4fs | f/%.1f | %.0fmm",
                    m.iso_speed, m.shutter_speed, m.aperture, m.focal_length_mm);
                ImGui::Text("%s", m.datetime_original.c_str());
            }
        }
        ImGui::End();
        }

        // ── Viewport / Grid (single center panel, content switches by mode) ──
        {
        bool zero_pad = (g_app_mode == AppMode::Develop);
        if (zero_pad)
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

        char vp_title[128];
        snprintf(vp_title, sizeof(vp_title), "%s###viewport", vega::tr(vega::S::PANEL_VIEWPORT));
        ImGui::Begin(vp_title);

        if (g_app_mode == AppMode::Library) {
            if (g_database.isOpen()) {
                g_grid_view.render(g_database, g_thumb_cache);
            } else {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                const char* hint = vega::tr(vega::S::FOLDER_ADD);
                ImVec2 ts = ImGui::CalcTextSize(hint);
                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::TextDisabled("%s", hint);
            }
        } else {
            if (g_has_image && g_image_srv)
            {
                if (g_show_before_after && g_before_srv)
                    g_before_after.render(g_before_srv.Get(), g_image_srv.Get(),
                                          g_raw_image.width, g_raw_image.height);
                else
                    g_viewport.render(g_image_srv.Get(), g_raw_image.width, g_raw_image.height);
            }
            else
            {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                const char* welcome = vega::tr(vega::S::STATUS_OPEN_HINT);
                ImVec2 ts = ImGui::CalcTextSize(welcome);
                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::TextDisabled("%s", welcome);
            }
        }

        ImGui::End();

        if (zero_pad)
            ImGui::PopStyleVar();
        }

        // ── Histogram (always rendered for docking) ──
        {
        char hist_title[128];
        snprintf(hist_title, sizeof(hist_title), "%s###histogram", vega::tr(vega::S::PANEL_HISTOGRAM));
        ImGui::Begin(hist_title);
        if (g_app_mode == AppMode::Develop) {
            g_histogram.render();
        }
        ImGui::End();
        }

        // ── Export Dialog ──
        if (g_app_mode == AppMode::Develop && g_export_dialog.isOpen() && g_has_image)
            if (g_rgba_ptr) {
                if (g_rgba_ptr->size() != static_cast<size_t>(g_raw_image.width) * g_raw_image.height * 4) {
                    static std::vector<uint8_t> s_export_buf;
                    const auto& full = g_pipeline.process(g_raw_image, g_recipe);
                    s_export_buf.assign(full.begin(), full.end());
                    g_export_dialog.render(s_export_buf, g_raw_image.width, g_raw_image.height);
                } else {
                    g_export_dialog.render(*g_rgba_ptr, g_raw_image.width, g_raw_image.height);
                }
            }

        // ── Status Bar ──
        {
            g_status_bar.zoom_pct = g_viewport.zoom() * 100.0f;
            g_status_bar.undo_current = g_history.currentIndex() + 1;
            g_status_bar.undo_total = g_history.totalEntries();
            g_status_bar.pipeline_ms = static_cast<float>(g_last_pipeline_ms);
            g_status_bar.use_gpu = g_use_gpu;
            g_status_bar.render();
        }

        // ── Render ──
        ImGui::Render();
        const float cc[4] = {0.08f, 0.08f, 0.10f, 1.0f};
        ID3D11RenderTargetView* rtv = g_ctx.rtv();
        g_ctx.context()->OMSetRenderTargets(1, &rtv, nullptr);
        g_ctx.context()->ClearRenderTargetView(rtv, cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_ctx.present(true);
    }

    // Save window state — use rcNormalPosition which is valid even when maximized
    {
        WINDOWPLACEMENT wp = {sizeof(wp)};
        GetWindowPlacement(g_hwnd, &wp);
        g_settings.maximized = (wp.showCmd == SW_SHOWMAXIMIZED);
        RECT& r = wp.rcNormalPosition;
        g_settings.window_x = r.left;
        g_settings.window_y = r.top;
        g_settings.window_w = r.right - r.left;
        g_settings.window_h = r.bottom - r.top;
        VEGA_LOG_INFO("Saving window state: {}x{} at ({},{}) maximized={}",
            g_settings.window_w, g_settings.window_h,
            g_settings.window_x, g_settings.window_y, g_settings.maximized);
        saveSettingsToDB();
    }

    // Cleanup
    VEGA_LOG_INFO("Shutting down...");
    g_bg_cancel = true;
    if (g_bg_thread.joinable()) g_bg_thread.join();
    if (g_import_thread.joinable()) g_import_thread.join();
    if (g_before_thread.joinable()) g_before_thread.join();
    g_thumb_cache.shutdown();
    g_database.close();
    g_ui_db.close();
    g_image_srv.Reset(); g_image_tex.Reset();
    g_before_srv.Reset(); g_before_tex.Reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    VEGA_LOG_DEBUG("ImGui shutdown");
    ImGui::DestroyContext();
    g_ctx.cleanup();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();
    VEGA_LOG_INFO("=== Vega shutdown complete ===");
    return 0;
}
