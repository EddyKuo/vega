// src/main.cpp — Phase 1: RAW decode + D3D11 display
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
#include "pipeline/SimplePipeline.h"
#include "ui/ImageViewport.h"

using Microsoft::WRL::ComPtr;

// ── Globals ──
static vega::D3D11Context g_ctx;
static vega::ImageViewport g_viewport;

// Current image state
static vega::RawImage g_raw_image;
static bool g_has_image = false;
static ComPtr<ID3D11ShaderResourceView> g_image_srv;
static ComPtr<ID3D11Texture2D> g_image_tex;
static std::string g_status_text = "Ready — Ctrl+O to open a RAW file";

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
        {
            g_ctx.resize(LOWORD(lParam), HIWORD(lParam));
        }
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
    {
        g_ctx.device()->CreateShaderResourceView(g_image_tex.Get(), nullptr, &g_image_srv);
    }
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

    std::filesystem::path filepath(filename);
    VEGA_LOG_INFO("Opening: {}", filepath.string());
    g_status_text = "Decoding " + filepath.filename().string() + "...";

    vega::Timer timer;

    // Decode RAW
    auto result = vega::RawDecoder::decode(filepath);
    if (!result)
    {
        VEGA_LOG_ERROR("Failed to decode RAW file");
        g_status_text = "Error: Failed to decode RAW file";
        return;
    }

    g_raw_image = std::move(result.value());
    double decode_ms = timer.elapsed_ms();
    VEGA_LOG_INFO("RAW decoded: {}x{} in {:.1f}ms",
                  g_raw_image.width, g_raw_image.height, decode_ms);

    // Enrich metadata with EXIF
    vega::ExifReader::enrichMetadata(filepath, g_raw_image.metadata);

    // Run CPU pipeline (demosaic → sRGB)
    timer.reset();
    auto rgba = vega::SimplePipeline::process(g_raw_image);
    double pipeline_ms = timer.elapsed_ms();
    VEGA_LOG_INFO("Pipeline: {:.1f}ms", pipeline_ms);

    // Upload to GPU texture
    uploadImageToGPU(rgba, g_raw_image.width, g_raw_image.height);
    g_has_image = true;

    g_status_text = filepath.filename().string() +
        " | " + g_raw_image.metadata.camera_make +
        " " + g_raw_image.metadata.camera_model +
        " | ISO " + std::to_string(g_raw_image.metadata.iso_speed) +
        " | " + std::to_string(g_raw_image.width) + "x" +
        std::to_string(g_raw_image.height) +
        " | Decode: " + std::to_string(static_cast<int>(decode_ms)) + "ms" +
        " | Pipeline: " + std::to_string(static_cast<int>(pipeline_ms)) + "ms";
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    vega::Logger::init();
    VEGA_LOG_INFO("Vega v0.1.0 starting...");

    // ── Window class ──
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

    // ── ImGui init ──
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

    VEGA_LOG_INFO("ImGui initialized with docking support");

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

        // Dockspace
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // ── Keyboard shortcuts ──
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O))
            openRawFile();

        // ── Develop Panel ──
        ImGui::Begin("Develop");
        {
            static float exposure    = 0.0f;
            static float contrast    = 0.0f;
            static float highlights  = 0.0f;
            static float shadows     = 0.0f;
            static float whites      = 0.0f;
            static float blacks      = 0.0f;
            static float temperature = 5500.0f;
            static float tint        = 0.0f;

            if (ImGui::CollapsingHeader("White Balance", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SliderFloat("Temperature", &temperature, 2000.0f, 12000.0f);
                ImGui::SliderFloat("Tint", &tint, -150.0f, 150.0f);
            }

            if (ImGui::CollapsingHeader("Tone", ImGuiTreeNodeFlags_DefaultOpen))
            {
                ImGui::SliderFloat("Exposure",   &exposure,   -5.0f, 5.0f);
                ImGui::SliderFloat("Contrast",   &contrast,   -100.0f, 100.0f);
                ImGui::SliderFloat("Highlights", &highlights, -100.0f, 100.0f);
                ImGui::SliderFloat("Shadows",    &shadows,    -100.0f, 100.0f);
                ImGui::SliderFloat("Whites",     &whites,     -100.0f, 100.0f);
                ImGui::SliderFloat("Blacks",     &blacks,     -100.0f, 100.0f);
            }

            if (g_has_image && ImGui::CollapsingHeader("Metadata", ImGuiTreeNodeFlags_DefaultOpen))
            {
                auto& m = g_raw_image.metadata;
                ImGui::Text("Camera: %s %s", m.camera_make.c_str(), m.camera_model.c_str());
                ImGui::Text("Lens: %s", m.lens_model.c_str());
                ImGui::Text("ISO: %u", m.iso_speed);
                ImGui::Text("Shutter: %.4fs", m.shutter_speed);
                ImGui::Text("Aperture: f/%.1f", m.aperture);
                ImGui::Text("Focal: %.0fmm", m.focal_length_mm);
                ImGui::Text("Date: %s", m.datetime_original.c_str());
                ImGui::Text("Size: %ux%u", g_raw_image.width, g_raw_image.height);
            }
        }
        ImGui::End();

        // ── Viewport ──
        ImGui::Begin("Viewport");
        {
            if (g_has_image && g_image_srv)
            {
                g_viewport.render(g_image_srv.Get(),
                                  g_raw_image.width, g_raw_image.height);
            }
            else
            {
                ImVec2 avail = ImGui::GetContentRegionAvail();
                ImVec2 text_size = ImGui::CalcTextSize("Ctrl+O to open a RAW file");
                ImGui::SetCursorPos(ImVec2(
                    (avail.x - text_size.x) * 0.5f,
                    (avail.y - text_size.y) * 0.5f));
                ImGui::TextDisabled("Ctrl+O to open a RAW file");
            }
        }
        ImGui::End();

        // ── Histogram ──
        ImGui::Begin("Histogram");
        ImGui::Text("Histogram placeholder");
        ImGui::End();

        // ── Status Bar ──
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            float status_h = ImGui::GetFrameHeight();
            ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x, vp->WorkPos.y + vp->WorkSize.y - status_h));
            ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, status_h));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
            ImGui::Begin("##StatusBar", nullptr,
                ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoDocking);
            ImGui::Text("%s", g_status_text.c_str());
            if (g_has_image)
            {
                ImGui::SameLine(ImGui::GetWindowWidth() - 120);
                ImGui::Text("Zoom: %.0f%%", g_viewport.zoom() * 100.0f);
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        // ── Render ──
        ImGui::Render();
        const float clear_color[4] = { 0.08f, 0.08f, 0.10f, 1.0f };
        ID3D11RenderTargetView* rtv = g_ctx.rtv();
        g_ctx.context()->OMSetRenderTargets(1, &rtv, nullptr);
        g_ctx.context()->ClearRenderTargetView(rtv, clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_ctx.present(true);
    }

    // ── Cleanup ──
    g_image_srv.Reset();
    g_image_tex.Reset();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    g_ctx.cleanup();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    VEGA_LOG_INFO("Vega shutdown complete");
    return 0;
}
