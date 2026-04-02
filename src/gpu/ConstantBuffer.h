#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>

using Microsoft::WRL::ComPtr;

namespace vega {

template<typename T>
class ConstantBuffer {
public:
    bool create(ID3D11Device* device)
    {
        if (!device)
            return false;

        D3D11_BUFFER_DESC desc{};
        // Constant buffer size must be a multiple of 16 bytes
        desc.ByteWidth      = (sizeof(T) + 15u) & ~15u;
        desc.Usage           = D3D11_USAGE_DYNAMIC;
        desc.BindFlags       = D3D11_BIND_CONSTANT_BUFFER;
        desc.CPUAccessFlags  = D3D11_CPU_ACCESS_WRITE;
        desc.MiscFlags       = 0;
        desc.StructureByteStride = 0;

        HRESULT hr = device->CreateBuffer(&desc, nullptr, &buffer_);
        return SUCCEEDED(hr);
    }

    void update(ID3D11DeviceContext* ctx, const T& data)
    {
        if (!ctx || !buffer_)
            return;

        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = ctx->Map(buffer_.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            std::memcpy(mapped.pData, &data, sizeof(T));
            ctx->Unmap(buffer_.Get(), 0);
        }
    }

    void bindCS(ID3D11DeviceContext* ctx, uint32_t slot) const
    {
        if (!ctx || !buffer_)
            return;

        ID3D11Buffer* bufs[] = { buffer_.Get() };
        ctx->CSSetConstantBuffers(slot, 1, bufs);
    }

    ID3D11Buffer* get() const { return buffer_.Get(); }
    bool isValid() const { return buffer_ != nullptr; }

private:
    ComPtr<ID3D11Buffer> buffer_;
};

} // namespace vega
