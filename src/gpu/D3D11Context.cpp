#include "gpu/D3D11Context.h"
#include "core/Logger.h"

#include <d3d11_1.h>
#include <dxgi1_2.h>

namespace vega
{

// ── Public ──────────────────────────────────────────────────────────────────

bool D3D11Context::initialize(HWND hwnd, uint32_t width, uint32_t height)
{
    // ── Feature levels ──
    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0
    };

    UINT create_flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    create_flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    // ── Create base device ──
    ComPtr<ID3D11Device>        base_device;
    ComPtr<ID3D11DeviceContext>  base_context;
    D3D_FEATURE_LEVEL           achieved_level{};

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        create_flags,
        feature_levels, _countof(feature_levels),
        D3D11_SDK_VERSION,
        &base_device,
        &achieved_level,
        &base_context);

    // If debug layer is unavailable, retry without it
    if (FAILED(hr) && (create_flags & D3D11_CREATE_DEVICE_DEBUG))
    {
        VEGA_LOG_WARN("D3D11 debug layer unavailable, retrying without it");
        create_flags &= ~D3D11_CREATE_DEVICE_DEBUG;
        hr = D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            create_flags,
            feature_levels, _countof(feature_levels),
            D3D11_SDK_VERSION,
            &base_device,
            &achieved_level,
            &base_context);
    }

    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("D3D11CreateDevice failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    VEGA_LOG_INFO("D3D11 device created (feature level {}.{})",
        (achieved_level >> 12) & 0xF,
        (achieved_level >> 8) & 0xF);

    // ── Query ID3D11Device1 / DeviceContext1 interfaces ──
    hr = base_device.As(&device_);
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("Failed to query ID3D11Device1: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    hr = base_context.As(&context_);
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("Failed to query ID3D11DeviceContext1: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    // ── Enable debug layer break-on-error in debug builds ──
#ifdef _DEBUG
    enableDebugLayer();
#endif

    // ── Obtain DXGI factory through the device ──
    ComPtr<IDXGIDevice1> dxgi_device;
    hr = device_.As(&dxgi_device);
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("Failed to query IDXGIDevice1: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    ComPtr<IDXGIAdapter> adapter;
    hr = dxgi_device->GetAdapter(&adapter);
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("GetAdapter failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    ComPtr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("GetParent(IDXGIFactory2) failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    // ── Create swap chain (FLIP_DISCARD, RGBA8, double-buffered) ──
    DXGI_SWAP_CHAIN_DESC1 scd{};
    scd.Width       = width;
    scd.Height      = height;
    scd.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.SampleDesc  = { 1, 0 };
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.BufferCount = 2;
    scd.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;
    scd.Flags       = 0;

    hr = factory->CreateSwapChainForHwnd(
        device_.Get(), hwnd, &scd, nullptr, nullptr, &swapchain_);
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("CreateSwapChainForHwnd failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return false;
    }

    // ── Disable Alt+Enter fullscreen toggle ──
    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    // ── Create initial render target view ──
    createRTV();
    if (!rtv_)
    {
        VEGA_LOG_ERROR("Failed to create initial RTV");
        return false;
    }

    // ── Log GPU info ──
    DXGI_ADAPTER_DESC adapter_desc{};
    adapter->GetDesc(&adapter_desc);

    char gpu_name[256]{};
    WideCharToMultiByte(CP_UTF8, 0,
        adapter_desc.Description, -1,
        gpu_name, sizeof(gpu_name),
        nullptr, nullptr);

    VEGA_LOG_INFO("GPU: {}", gpu_name);
    VEGA_LOG_INFO("Dedicated VRAM: {} MB",
        adapter_desc.DedicatedVideoMemory / (1024 * 1024));
    VEGA_LOG_INFO("Shared system memory: {} MB",
        adapter_desc.SharedSystemMemory / (1024 * 1024));

    return true;
}

void D3D11Context::resize(uint32_t width, uint32_t height)
{
    if (!swapchain_ || width == 0 || height == 0)
        return;

    // Release the current RTV so the back buffer ref count drops to zero
    rtv_.Reset();

    HRESULT hr = swapchain_->ResizeBuffers(
        0,              // preserve existing buffer count
        width, height,
        DXGI_FORMAT_UNKNOWN,  // preserve existing format
        0);

    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("ResizeBuffers failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return;
    }

    createRTV();
}

void D3D11Context::present(bool vsync)
{
    if (!swapchain_)
        return;

    UINT sync_interval = vsync ? 1 : 0;
    UINT flags = 0;

    HRESULT hr = swapchain_->Present(sync_interval, flags);
    if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
    {
        VEGA_LOG_ERROR("Device lost during Present: 0x{:08X}", static_cast<uint32_t>(hr));
        // A full device recovery path would go here; for now, log the error.
    }
}

void D3D11Context::cleanup()
{
    // Reset in reverse order of creation
    rtv_.Reset();
    swapchain_.Reset();
    context_.Reset();
    device_.Reset();

    VEGA_LOG_INFO("D3D11Context cleaned up");
}

// ── Texture / View helpers ──────────────────────────────────────────────────

ComPtr<ID3D11Texture2D> D3D11Context::createTexture2D(
    uint32_t width, uint32_t height,
    DXGI_FORMAT format, bool uav, bool srv)
{
    if (!device_)
    {
        VEGA_LOG_ERROR("createTexture2D called with null device");
        return nullptr;
    }

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width      = width;
    desc.Height     = height;
    desc.MipLevels  = 1;
    desc.ArraySize  = 1;
    desc.Format     = format;
    desc.SampleDesc = { 1, 0 };
    desc.Usage      = D3D11_USAGE_DEFAULT;
    desc.BindFlags  = 0;

    if (srv)
        desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
    if (uav)
        desc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &texture);
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("CreateTexture2D ({}x{}, fmt={}) failed: 0x{:08X}",
            width, height, static_cast<uint32_t>(format), static_cast<uint32_t>(hr));
        return nullptr;
    }

    return texture;
}

ComPtr<ID3D11ShaderResourceView> D3D11Context::createSRV(ID3D11Texture2D* tex)
{
    if (!device_ || !tex)
    {
        VEGA_LOG_ERROR("createSRV called with null device or texture");
        return nullptr;
    }

    // Derive format from the texture description
    D3D11_TEXTURE2D_DESC tex_desc{};
    tex->GetDesc(&tex_desc);

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format                    = tex_desc.Format;
    srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels       = tex_desc.MipLevels;

    ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = device_->CreateShaderResourceView(tex, &srv_desc, &srv);
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("CreateShaderResourceView failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return nullptr;
    }

    return srv;
}

ComPtr<ID3D11UnorderedAccessView> D3D11Context::createUAV(ID3D11Texture2D* tex)
{
    if (!device_ || !tex)
    {
        VEGA_LOG_ERROR("createUAV called with null device or texture");
        return nullptr;
    }

    // Derive format from the texture description
    D3D11_TEXTURE2D_DESC tex_desc{};
    tex->GetDesc(&tex_desc);

    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format             = tex_desc.Format;
    uav_desc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D.MipSlice = 0;

    ComPtr<ID3D11UnorderedAccessView> uav;
    HRESULT hr = device_->CreateUnorderedAccessView(tex, &uav_desc, &uav);
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("CreateUnorderedAccessView failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return nullptr;
    }

    return uav;
}

// ── Private ─────────────────────────────────────────────────────────────────

void D3D11Context::createRTV()
{
    ComPtr<ID3D11Texture2D> back_buffer;
    HRESULT hr = swapchain_->GetBuffer(0, IID_PPV_ARGS(&back_buffer));
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("GetBuffer(0) failed: 0x{:08X}", static_cast<uint32_t>(hr));
        return;
    }

    hr = device_->CreateRenderTargetView(back_buffer.Get(), nullptr, &rtv_);
    if (FAILED(hr))
    {
        VEGA_LOG_ERROR("CreateRenderTargetView failed: 0x{:08X}", static_cast<uint32_t>(hr));
    }
}

void D3D11Context::enableDebugLayer()
{
    ComPtr<ID3D11Debug> debug;
    HRESULT hr = device_.As(&debug);
    if (SUCCEEDED(hr))
    {
        ComPtr<ID3D11InfoQueue> info_queue;
        hr = debug.As(&info_queue);
        if (SUCCEEDED(hr))
        {
            info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            info_queue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
            VEGA_LOG_DEBUG("D3D11 debug break-on-error enabled");
        }
    }
}

} // namespace vega
