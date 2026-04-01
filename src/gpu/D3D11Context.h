#pragma once
#include <d3d11_1.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace vega
{

class D3D11Context
{
public:
    bool initialize(HWND hwnd, uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);
    void present(bool vsync = true);
    void cleanup();

    ID3D11Device*           device()    const { return device_.Get(); }
    ID3D11DeviceContext*    context()   const { return context_.Get(); }
    ID3D11RenderTargetView* rtv()      const { return rtv_.Get(); }
    IDXGISwapChain1*        swapchain() const { return swapchain_.Get(); }

    ComPtr<ID3D11Texture2D> createTexture2D(
        uint32_t width, uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT,
        bool uav = false, bool srv = true);
    ComPtr<ID3D11ShaderResourceView>  createSRV(ID3D11Texture2D* tex);
    ComPtr<ID3D11UnorderedAccessView> createUAV(ID3D11Texture2D* tex);

private:
    ComPtr<ID3D11Device>           device_;
    ComPtr<ID3D11DeviceContext>    context_;
    ComPtr<IDXGISwapChain1>        swapchain_;
    ComPtr<ID3D11RenderTargetView> rtv_;

    void createRTV();
    void enableDebugLayer();
};

} // namespace vega
