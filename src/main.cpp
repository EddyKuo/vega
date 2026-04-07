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
#include "core/i18n.h"
#include "pipeline/GPUPipeline.h"

using Microsoft::WRL::ComPtr;

// ── Globals ──
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

    g_bg_cancel = false;
    g_bg_running = true;
    g_bg_ready = false;
    g_bg_has_pending = false;

    vega::EditRecipe recipe_copy = g_recipe;
    std::thread([recipe_copy]() {
        vega::Timer timer;
        const auto& rgba = g_bg_pipeline.process(g_raw_image, recipe_copy);
        if (g_bg_cancel) {
            // Cancelled — check if there's a pending recipe to process instead
            g_bg_running = false;
            return;
        }
        {
            std::lock_guard<std::mutex> lock(g_bg_mutex);
            g_bg_result.assign(rgba.begin(), rgba.end());
            g_bg_w = g_raw_image.width;
            g_bg_h = g_raw_image.height;
            g_bg_pipeline_ms = timer.elapsed_ms();
        }
        g_bg_ready = true;
        g_bg_running = false;
    }).detach();
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

// CPU preview on UI thread (1/8 res, skip heavy nodes)
static void reprocessPreview()
{
    if (!g_has_image) return;
    vega::Timer timer;

    // Use a lightweight recipe for preview — skip denoise/sharpen
    // (they're barely visible at 1/8 res and cost 300ms+ each)
    vega::EditRecipe preview_recipe = g_recipe;
    preview_recipe.denoise_luminance = 0;
    preview_recipe.denoise_color = 0;
    preview_recipe.sharpen_amount = 0;

    uint32_t pw, ph;
    const auto& rgba = g_pipeline.processPreview(g_raw_image, preview_recipe, 8, pw, ph);
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
            g_display_w = g_raw_image.width;
            g_display_h = g_raw_image.height;
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

static void generateBeforeImage()
{
    if (!g_has_image) return;
    g_before_rgba = g_pipeline.process(g_raw_image, vega::EditRecipe{});
    uploadToGPU(g_before_rgba, g_raw_image.width, g_raw_image.height, g_before_tex, g_before_srv);
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

    g_current_path = std::filesystem::path(filename);
    g_settings.last_open_dir = g_current_path.parent_path().string();

    VEGA_LOG_INFO("Opening: {}", g_current_path.string());
    vega::Timer timer;

    auto result = vega::RawDecoder::decode(g_current_path);
    if (!result)
    {
        g_status_bar.filename = "Error: Failed to decode RAW file";
        return;
    }

    g_raw_image = std::move(result.value());
    double decode_ms = timer.elapsed_ms();
    VEGA_LOG_INFO("Decoded {}x{} in {:.1f}ms (black={}, white={})",
        g_raw_image.width, g_raw_image.height, decode_ms,
        g_raw_image.black_level, g_raw_image.white_level);

    vega::ExifReader::enrichMetadata(g_current_path, g_raw_image.metadata);
    VEGA_LOG_INFO("EXIF: {} {} | ISO {} | {:.4f}s | f/{:.1f} | {}mm",
        g_raw_image.metadata.camera_make, g_raw_image.metadata.camera_model,
        g_raw_image.metadata.iso_speed, g_raw_image.metadata.shutter_speed,
        g_raw_image.metadata.aperture, g_raw_image.metadata.focal_length_mm);

    auto saved = vega::loadRecipe(g_current_path);
    if (saved) VEGA_LOG_INFO("Loaded .vgr sidecar recipe");
    g_recipe = saved ? *saved : vega::EditRecipe{};
    g_history.clear();
    g_has_image = true;

    // Upload to GPU if available
    if (g_use_gpu)
        g_gpu_pipeline.uploadRawData(g_raw_image);

    vega::Timer pipeline_timer;
    reprocessPipeline();
    VEGA_LOG_INFO("Initial pipeline: {:.1f}ms", pipeline_timer.elapsed_ms());
    generateBeforeImage();

    // Fit image to viewport on first load
    ImVec2 vp_size = ImGui::GetMainViewport()->WorkSize;
    g_viewport.fitToWindow(ImVec2(vp_size.x * 0.65f, vp_size.y * 0.8f),
                           g_raw_image.width, g_raw_image.height);

    auto& m = g_raw_image.metadata;
    g_status_bar.filename = g_current_path.filename().string();
    g_status_bar.camera_info = m.camera_make + " " + m.camera_model;
    g_status_bar.resolution = std::to_string(g_raw_image.width) + "x" + std::to_string(g_raw_image.height);
    g_status_bar.pipeline_ms = decode_ms;

    // Update window title
    std::wstring title = L"Vega - " +
        std::wstring(g_current_path.filename().wstring());
    SetWindowTextW(g_hwnd, title.c_str());
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
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->WorkSize);

    // Split: left 75% (viewport area) | right 25% (develop panel)
    ImGuiID dock_right;
    ImGuiID dock_left = ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.75f, nullptr, &dock_right);

    // Split right: top 70% (develop) | bottom 30% (histogram)
    ImGuiID dock_right_bottom;
    ImGuiID dock_right_top = ImGui::DockBuilderSplitNode(dock_right, ImGuiDir_Up, 0.70f, nullptr, &dock_right_bottom);

    // Dock windows
    // Must match the exact strings passed to ImGui::Begin()
    // Format: "DisplayTitle###fixed_id"
    char buf[128];
    snprintf(buf, sizeof(buf), "%s###viewport", vega::tr(vega::S::PANEL_VIEWPORT));
    ImGui::DockBuilderDockWindow(buf, dock_left);
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
            if (ImGui::MenuItem(tr(S::MENU_BEFORE_AFTER), "B", g_show_before_after))
                g_show_before_after = !g_show_before_after;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu(tr(S::MENU_LANGUAGE)))
        {
            bool is_en = I18n::instance().language() == Lang::EN;
            bool is_zh = I18n::instance().language() == Lang::ZH_TW;
            if (ImGui::MenuItem("English", nullptr, is_en))
            {
                I18n::instance().setLanguage(Lang::EN);
                g_settings.language = "en";
            }
            if (ImGui::MenuItem("\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87", nullptr, is_zh))
            {
                I18n::instance().setLanguage(Lang::ZH_TW);
                g_settings.language = "zh_tw";
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    vega::installCrashHandler();
    vega::WindowsIntegration::enableHighDPI();
    vega::Logger::init();
    VEGA_LOG_INFO("=== Vega v0.1.0 starting ===");
    VEGA_LOG_INFO("Log file: vega.log");

    g_settings = vega::AppSettings::load();
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
    io.IniFilename = "vega_imgui.ini";

    applyVegaTheme();

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

    // Init i18n from settings
    if (g_settings.language == "zh_tw")
        vega::I18n::instance().setLanguage(vega::Lang::ZH_TW);
    else
        vega::I18n::instance().setLanguage(vega::Lang::EN);

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
    tb_cb.on_toggle_before_after = []{ g_show_before_after = !g_show_before_after; };
    tb_cb.on_mode_grid = []{};
    tb_cb.on_mode_develop = []{};
    g_toolbar.setCallbacks(tb_cb);

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

        // Poll background pipeline result + relaunch if pending
        pollBackgroundResult();
        checkPendingBackground();

        // Menu bar
        renderMenuBar();

        // Dockspace
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Set up default layout on first frame (if no saved layout)
        if (g_first_frame)
        {
            g_first_frame = false;
            // Only build layout if no saved ini exists
            if (ImGui::DockBuilderGetNode(dockspace_id) == nullptr ||
                !std::filesystem::exists("vega_imgui.ini"))
            {
                buildDefaultLayout(dockspace_id);
            }
        }

        // Keyboard shortcuts
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
            openRawFile();
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
        if (ImGui::IsKeyPressed(ImGuiKey_B, false) && !io.KeyCtrl && !io.KeyAlt)
            g_show_before_after = !g_show_before_after;
        if (ImGui::IsKeyPressed(ImGuiKey_M, false) && g_show_before_after)
            g_before_after.toggleMode();
        if (g_show_before_after)
            g_before_after.setShowBefore(ImGui::IsKeyDown(ImGuiKey_Backslash));
        // W: WB eyedropper
        if (ImGui::IsKeyPressed(ImGuiKey_W, false) && !io.KeyCtrl && g_has_image)
            g_viewport.activateEyedropper();

        // ── Develop Panel ──
        {
        char dev_title[128];
        snprintf(dev_title, sizeof(dev_title), "%s###develop", vega::tr(vega::S::PANEL_DEVELOP));
        ImGui::Begin(dev_title);
        {
            if (g_develop_panel.render(g_recipe, g_history))
                reprocessPipeline();

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

        // ── Viewport ──
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        {
        char vp_title[128];
        snprintf(vp_title, sizeof(vp_title), "%s###viewport", vega::tr(vega::S::PANEL_VIEWPORT));
        ImGui::Begin(vp_title);
        {
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
                // Center welcome text
                const char* welcome = vega::tr(vega::S::STATUS_OPEN_HINT);
                ImVec2 ts = ImGui::CalcTextSize(welcome);
                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::TextDisabled("%s", welcome);
            }
        }
        ImGui::End();
        }
        ImGui::PopStyleVar();

        // ── Histogram ──
        {
        char hist_title[128];
        snprintf(hist_title, sizeof(hist_title), "%s###histogram", vega::tr(vega::S::PANEL_HISTOGRAM));
        ImGui::Begin(hist_title);
        g_histogram.render();
        ImGui::End();
        }

        // ── Export Dialog ──
        if (g_export_dialog.isOpen() && g_has_image)
            if (g_rgba_ptr)
                g_export_dialog.render(*g_rgba_ptr, g_raw_image.width, g_raw_image.height);

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
        g_settings.save();
    }

    // Cleanup
    VEGA_LOG_INFO("Shutting down...");
    g_image_srv.Reset(); g_image_tex.Reset();
    g_before_srv.Reset(); g_before_tex.Reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    VEGA_LOG_DEBUG("ImGui shutdown");
    ImGui::DestroyContext();
    g_ctx.cleanup();
    DestroyWindow(g_hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);
    VEGA_LOG_INFO("=== Vega shutdown complete ===");
    return 0;
}
