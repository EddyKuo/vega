#pragma once
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <cstdint>

using Microsoft::WRL::ComPtr;

namespace vega {

class TexturePool {
public:
    explicit TexturePool(ID3D11Device* device);

    struct TextureHandle {
        ComPtr<ID3D11Texture2D> texture;
        ComPtr<ID3D11ShaderResourceView> srv;
        ComPtr<ID3D11UnorderedAccessView> uav;
        uint32_t width = 0, height = 0;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

        bool isValid() const { return texture != nullptr; }
        void reset()
        {
            texture.Reset();
            srv.Reset();
            uav.Reset();
            width = height = 0;
            format = DXGI_FORMAT_UNKNOWN;
        }
    };

    // Acquire a texture matching specs. Reuses from pool if available.
    TextureHandle acquire(uint32_t width, uint32_t height,
                          DXGI_FORMAT format = DXGI_FORMAT_R32G32B32A32_FLOAT);

    // Release texture back to pool for reuse
    void release(TextureHandle& handle);

    // Clear all cached textures
    void clear();

    // Number of textures currently held in the pool (both free and in-use)
    size_t poolSize() const;

    // Number of textures currently available for reuse
    size_t freeCount() const;

private:
    ID3D11Device* device_;

    struct PoolEntry {
        TextureHandle handle;
        bool in_use;
    };
    std::vector<PoolEntry> pool_;

    // Create a brand new texture + views
    TextureHandle createTexture(uint32_t width, uint32_t height, DXGI_FORMAT format);

    // Check if an existing pool entry matches the requested specs
    static bool matches(const PoolEntry& entry, uint32_t w, uint32_t h, DXGI_FORMAT fmt);
};

} // namespace vega
