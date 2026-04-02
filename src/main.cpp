// src/main.cpp — Phase 4: Before/After, Export, Keyboard Shortcuts
#define NOMINMAX
#include <windows.h>
#include <commdlg.h>
#include <d3d11.h>
#include <wrl/client.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include "core/Logger.h"
#include "core/Timer.h"
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

using Microsoft::WRL::ComPtr;

// ── Globals ──
static vega::D3D11Context g_ctx;
static vega::ImageViewport g_viewport;
static vega::DevelopPanel g_develop_panel;
static vega::HistogramView g_histogram;
static vega::Pipeline g_pipeline;
static vega::EditRecipe g_recipe;
static vega::EditHistory g_history;
static vega::BeforeAfter g_before_after;
static vega::ExportDialog g_export_dialog;

// Current image state
static vega::RawImage g_raw_image;
static bool g_has_image = false;
static ComPtr<ID3D11ShaderResourceView> g_image_srv;
static ComPtr<ID3D11Texture2D> g_image_tex;
static std::string g_status_text = "Ready — Ctrl+O to open a RAW file";
static std::filesystem::path g_current_path;
static std::vector<uint8_t> g_rgba_buffer;

// Before/After state
static ComPtr<ID3D11ShaderResourceView> g_before_srv;
static ComPtr<ID3D11Texture2D> g_before_tex;
static std::vector<uint8_t> g_before_rgba;
static bool g_show_before_after = false;

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
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
}

// Upload RGBA8 buffer to D3D11 texture + SRV
static void uploadImageToGPU(const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = {1, 0};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = rgba.data();
    init.SysMemPitch = w * 4;

    g_image_tex.Reset();
    g_image_srv.Reset();
    HRESULT hr = g_ctx.device()->CreateTexture2D(&desc, &init, &g_image_tex);
    if (SUCCEEDED(hr))
        g_ctx.device()->CreateShaderResourceView(g_image_tex.Get(), nullptr, &g_image_srv);
}

// Upload RGBA8 buffer to a specified D3D11 texture + SRV pair
static void uploadToGPU(const std::vector<uint8_t>& rgba, uint32_t w, uint32_t h,
                        ComPtr<ID3D11Texture2D>& tex, ComPtr<ID3D11ShaderResourceView>& srv)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = {1, 0};
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = rgba.data();
    init.SysMemPitch = w * 4;

    tex.Reset();
    srv.Reset();
    HRESULT hr = g_ctx.device()->CreateTexture2D(&desc, &init, &tex);
    if (SUCCEEDED(hr))
        g_ctx.device()->CreateShaderResourceView(tex.Get(), nullptr, &srv);
}

// Generate the "before" image (default recipe) for comparison
static void generateBeforeImage()
{
    if (!g_has_image) return;
    vega::EditRecipe default_recipe;
    g_before_rgba = g_pipeline.process(g_raw_image, default_recipe);
    uploadToGPU(g_before_rgba, g_raw_image.width, g_raw_image.height, g_before_tex, g_before_srv);
}

// Re-process the pipeline with current recipe and update display
static void reprocessPipeline()
{
    if (!g_has_image) return;

    vega::Timer timer;
    g_rgba_buffer = g_pipeline.process(g_raw_image, g_recipe);
    double ms = timer.elapsed_ms();

    uploadImageToGPU(g_rgba_buffer, g_raw_image.width, g_raw_image.height);
    g_histogram.compute(g_rgba_buffer.data(), g_raw_image.width, g_raw_image.height);

    VEGA_LOG_DEBUG("Pipeline: {:.1f}ms", ms);
}

// Open file dialog and load RAW
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

    if (!GetOpenFileNameW(&ofn))
        return;

    g_current_path = std::filesystem::path(filename);
    VEGA_LOG_INFO("Opening: {}", g_current_path.string());

    vega::Timer timer;

    // Decode RAW
    auto result = vega::RawDecoder::decode(g_current_path);
    if (!result)
    {
        VEGA_LOG_ERROR("Failed to decode RAW file");
        g_status_text = "Error: Failed to decode RAW file";
        return;
    }

    g_raw_image = std::move(result.value());
    double decode_ms = timer.elapsed_ms();

    // Enrich metadata
    vega::ExifReader::enrichMetadata(g_current_path, g_raw_image.metadata);

    // Load existing recipe or start fresh
    auto saved = vega::loadRecipe(g_current_path);
    if (saved)
    {
        g_recipe = *saved;
        VEGA_LOG_INFO("Loaded .vgr sidecar");
    }
    else
    {
        g_recipe = vega::EditRecipe{};
        g_recipe.wb_temperature = 5500.0f; // Will use camera WB by default
    }

    g_history.clear();
    g_has_image = true;

    // Process pipeline
    reprocessPipeline();

    // Generate "before" image for comparison (default recipe)
    generateBeforeImage();

    g_status_text = g_current_path.filename().string() +
        " | " + g_raw_image.metadata.camera_make +
        " " + g_raw_image.metadata.camera_model +
        " | " + std::to_string(g_raw_image.width) + "x" +
        std::to_string(g_raw_image.height) +
        " | Decode: " + std::to_string(static_cast<int>(decode_ms)) + "ms";
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    vega::Logger::init();
    VEGA_LOG_INFO("Vega v0.1.0 starting...");

    // ── Window ──
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(WNDCLASSEXW);
    wc.style         = CS_CLASSDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = L"VegaEditor";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Vega v0.1",
        WS_OVERLAPPEDWINDOW, 100, 100, 1920, 1080,
        nullptr, nullptr, hInst, nullptr);

    if (!g_ctx.initialize(hwnd, 1920, 1080))
    {
        VEGA_LOG_ERROR("Failed to initialize D3D11");
        return 1;
    }

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ── ImGui ──
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.IniFilename = "vega_imgui.ini";

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 4.0f;
    style.FrameRounding     = 2.0f;
    style.GrabRounding      = 2.0f;
    style.ScrollbarRounding = 4.0f;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_ctx.device(), g_ctx.context());

    // ── Main loop ──
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

        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // ── Keyboard shortcuts ──
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
            openRawFile();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && g_has_image)
            vega::saveRecipe(g_current_path, g_recipe);
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && g_history.canUndo())
        {
            g_recipe = g_history.undo();
            reprocessPipeline();
        }
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y) && g_history.canRedo())
        {
            g_recipe = g_history.redo();
            reprocessPipeline();
        }

        // Ctrl+Shift+E: Export
        if (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_E) && g_has_image)
            g_export_dialog.open(g_current_path);

        // B: Toggle before/after view
        if (ImGui::IsKeyPressed(ImGuiKey_B, false) && !io.KeyCtrl && !io.KeyAlt)
            g_show_before_after = !g_show_before_after;

        // M: Cycle before/after mode (when in before/after view)
        if (ImGui::IsKeyPressed(ImGuiKey_M, false) && g_show_before_after)
            g_before_after.toggleMode();

        // Backslash: Hold to show "before" in Toggle mode
        if (g_show_before_after)
            g_before_after.setShowBefore(ImGui::IsKeyDown(ImGuiKey_Backslash));

        // R: Reset all edits to default
        if (ImGui::IsKeyPressed(ImGuiKey_R, false) && io.KeyCtrl && io.KeyShift && g_has_image)
        {
            vega::EditCommand reset_cmd;
            reset_cmd.description = "Reset All";
            reset_cmd.before = g_recipe;
            reset_cmd.after = vega::EditRecipe{};
            reset_cmd.affected_stage = vega::PipelineStage::All;
            g_history.push(reset_cmd);
            g_recipe = reset_cmd.after;
            reprocessPipeline();
        }

        // ── Develop Panel ──
        ImGui::Begin("Develop");
        {
            if (g_develop_panel.render(g_recipe, g_history))
                reprocessPipeline();

            if (g_has_image && ImGui::CollapsingHeader("Metadata"))
            {
                auto& m = g_raw_image.metadata;
                ImGui::Text("Camera: %s %s", m.camera_make.c_str(), m.camera_model.c_str());
                ImGui::Text("Lens: %s", m.lens_model.c_str());
                ImGui::Text("ISO: %u  Shutter: %.4fs  f/%.1f", m.iso_speed, m.shutter_speed, m.aperture);
                ImGui::Text("Focal: %.0fmm  Date: %s", m.focal_length_mm, m.datetime_original.c_str());
            }
        }
        ImGui::End();

        // ── Viewport ──
        ImGui::Begin("Viewport");
        {
            if (g_has_image && g_image_srv)
            {
                if (g_show_before_after && g_before_srv)
                {
                    g_before_after.render(g_before_srv.Get(), g_image_srv.Get(),
                                          g_raw_image.width, g_raw_image.height);
                }
                else
                {
                    g_viewport.render(g_image_srv.Get(), g_raw_image.width, g_raw_image.height);
                }
            }
            else
            {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                ImVec2 ts = ImGui::CalcTextSize("Ctrl+O to open a RAW file");
                ImGui::SetCursorPos(ImVec2((avail.x - ts.x) * 0.5f, (avail.y - ts.y) * 0.5f));
                ImGui::TextDisabled("Ctrl+O to open a RAW file");
            }
        }
        ImGui::End();

        // ── Export Dialog ──
        if (g_export_dialog.isOpen() && g_has_image)
            g_export_dialog.render(g_rgba_buffer, g_raw_image.width, g_raw_image.height);

        // ── Histogram ──
        ImGui::Begin("Histogram");
        g_histogram.render();
        ImGui::End();

        // ── Status Bar ──
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            float sh = ImGui::GetFrameHeight();
            ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - sh));
            ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, sh));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
            ImGui::Begin("##StatusBar", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking);
            ImGui::Text("%s", g_status_text.c_str());
            if (g_has_image)
            {
                ImGui::SameLine(ImGui::GetWindowWidth() - 400);
                if (g_show_before_after)
                {
                    const char* mode_str = "Split";
                    switch (g_before_after.mode()) {
                    case vega::BeforeAfter::Mode::SideBySide: mode_str = "Side"; break;
                    case vega::BeforeAfter::Mode::SplitView:  mode_str = "Split"; break;
                    case vega::BeforeAfter::Mode::Toggle:     mode_str = "Toggle"; break;
                    }
                    ImGui::Text("B/A: %s  |  Zoom: %.0f%%  |  Undo: %d/%d",
                        mode_str,
                        g_viewport.zoom() * 100.0f,
                        g_history.currentIndex() + 1, g_history.totalEntries());
                }
                else
                {
                    ImGui::Text("Zoom: %.0f%%  |  Undo: %d/%d",
                        g_viewport.zoom() * 100.0f,
                        g_history.currentIndex() + 1, g_history.totalEntries());
                }
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        // ── Render ──
        ImGui::Render();
        const float cc[4] = { 0.08f, 0.08f, 0.10f, 1.0f };
        ID3D11RenderTargetView* rtv = g_ctx.rtv();
        g_ctx.context()->OMSetRenderTargets(1, &rtv, nullptr);
        g_ctx.context()->ClearRenderTargetView(rtv, cc);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_ctx.present(true);
    }

    // ── Cleanup ──
    g_image_srv.Reset();
    g_image_tex.Reset();
    g_before_srv.Reset();
    g_before_tex.Reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_ctx.cleanup();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    VEGA_LOG_INFO("Vega shutdown complete");
    return 0;
}
