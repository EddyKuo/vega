// src/main.cpp — Phase 0: D3D11 + ImGui 最小驗證窗口
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include "core/Logger.h"

using Microsoft::WRL::ComPtr;

// ── Globals ──
static ComPtr<ID3D11Device>           g_device;
static ComPtr<ID3D11DeviceContext>    g_context;
static ComPtr<IDXGISwapChain1>        g_swapchain;
static ComPtr<ID3D11RenderTargetView> g_rtv;

// ── Forward declarations ──
static bool InitD3D11(HWND hwnd, uint32_t width, uint32_t height);
static void CleanupD3D11();
static void CreateRTV();

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
    HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
    case WM_SIZE:
        if (g_device && wParam != SIZE_MINIMIZED)
        {
            g_rtv.Reset();
            g_swapchain->ResizeBuffers(0,
                LOWORD(lParam), HIWORD(lParam),
                DXGI_FORMAT_UNKNOWN, 0);
            CreateRTV();
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hWnd, msg, wParam, lParam);
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
    wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(101));
    wc.lpszClassName = L"VegaEditor";
    RegisterClassExW(&wc);

    HWND hwnd = CreateWindowW(wc.lpszClassName, L"Vega v0.1",
        WS_OVERLAPPEDWINDOW, 100, 100, 1920, 1080,
        nullptr, nullptr, hInst, nullptr);

    if (!InitD3D11(hwnd, 1920, 1080))
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
    style.WindowRounding   = 4.0f;
    style.FrameRounding    = 2.0f;
    style.GrabRounding     = 2.0f;
    style.ScrollbarRounding = 4.0f;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_device.Get(), g_context.Get());

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

        // Dockspace over entire viewport
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // ── Demo: Develop panel ──
        ImGui::Begin("Develop");
        ImGui::Text("Vega — Phase 0 OK");
        ImGui::Separator();

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

        ImGui::End();

        // ── Demo: Viewport ──
        ImGui::Begin("Viewport");
        ImVec2 size = ImGui::GetContentRegionAvail();
        ImGui::Text("Image viewport: %.0f x %.0f", size.x, size.y);
        ImGui::End();

        // ── Demo: Histogram ──
        ImGui::Begin("Histogram");
        ImGui::Text("Histogram placeholder");
        ImGui::End();

        // ── Render ──
        ImGui::Render();
        const float clear_color[4] = { 0.08f, 0.08f, 0.10f, 1.0f };
        g_context->OMSetRenderTargets(1, g_rtv.GetAddressOf(), nullptr);
        g_context->ClearRenderTargetView(g_rtv.Get(), clear_color);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_swapchain->Present(1, 0);
    }

    // ── Cleanup ──
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupD3D11();
    DestroyWindow(hwnd);
    UnregisterClassW(wc.lpszClassName, wc.hInstance);

    VEGA_LOG_INFO("Vega shutdown complete");
    return 0;
}

// ── D3D11 Implementation ──

static void CreateRTV()
{
    ComPtr<ID3D11Texture2D> back_buffer;
    g_swapchain->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    g_device->CreateRenderTargetView(back_buffer.Get(), nullptr, &g_rtv);
}

static bool InitD3D11(HWND hwnd, uint32_t width, uint32_t height)
{
    // Create device
    D3D_FEATURE_LEVEL feature_levels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
    UINT create_flags = 0;
#ifdef _DEBUG
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    ComPtr<ID3D11Device> base_device;
    ComPtr<ID3D11DeviceContext> base_context;
    HRESULT hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        create_flags, feature_levels, _countof(feature_levels),
        D3D11_SDK_VERSION,
        &base_device, nullptr, &base_context);

    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("D3D11CreateDevice failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    base_device.As(&g_device);
    base_context.As(&g_context);

    // Create swap chain
    ComPtr<IDXGIDevice1> dxgi_device;
    g_device.As(&dxgi_device);

    ComPtr<IDXGIAdapter> adapter;
    dxgi_device->GetAdapter(&adapter);

    ComPtr<IDXGIFactory2> factory;
    adapter->GetParent(IID_PPV_ARGS(&factory));

    DXGI_SWAP_CHAIN_DESC1 scd = {};
    scd.Width       = width;
    scd.Height      = height;
    scd.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc  = { 1, 0 };
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    hr = factory->CreateSwapChainForHwnd(
        g_device.Get(), hwnd, &scd, nullptr, nullptr, &g_swapchain);

    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("CreateSwapChain failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    // Disable Alt+Enter fullscreen
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    CreateRTV();

    // Log GPU info
    DXGI_ADAPTER_DESC adapter_desc;
    adapter->GetDesc(&adapter_desc);
    char gpu_name[128];
    WideCharToMultiByte(CP_UTF8, 0, adapter_desc.Description, -1, gpu_name, 128, nullptr, nullptr);
    VEGA_LOG_INFO("GPU: {}", gpu_name);
    VEGA_LOG_INFO("VRAM: {} MB", adapter_desc.DedicatedVideoMemory / (1024 * 1024));

    return true;
}

static void CleanupD3D11()
{
    g_rtv.Reset();
    g_swapchain.Reset();
    g_context.Reset();
    g_device.Reset();
}
