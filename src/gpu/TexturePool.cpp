#include "gpu/TexturePool.h"
#include "core/Logger.h"

namespace vega {

TexturePool::TexturePool(ID3D11Device* device)
    : device_(device)
{
}

TexturePool::TextureHandle TexturePool::acquire(uint32_t width, uint32_t height,
                                                 DXGI_FORMAT format)
{
    // First, look for an existing free texture with matching dimensions/format
    for (auto& entry : pool_) {
        if (!entry.in_use && matches(entry, width, height, format)) {
            entry.in_use = true;
            VEGA_LOG_DEBUG("TexturePool: reused texture ({}x{}, fmt={}), pool size={}",
                           width, height, static_cast<uint32_t>(format), pool_.size());
            return entry.handle;
        }
    }

    // No match found -- create a new texture
    TextureHandle handle = createTexture(width, height, format);
    if (!handle.isValid()) {
        VEGA_LOG_ERROR("TexturePool: failed to create texture ({}x{}, fmt={})",
                       width, height, static_cast<uint32_t>(format));
        return handle;
    }

    pool_.push_back({ handle, true });
    VEGA_LOG_DEBUG("TexturePool: created new texture ({}x{}, fmt={}), pool size={}",
                   width, height, static_cast<uint32_t>(format), pool_.size());
    return handle;
}

void TexturePool::release(TextureHandle& handle)
{
    if (!handle.isValid())
        return;

    for (auto& entry : pool_) {
        // Match by raw texture pointer -- each texture is unique
        if (entry.handle.texture.Get() == handle.texture.Get()) {
            entry.in_use = false;
            VEGA_LOG_DEBUG("TexturePool: released texture ({}x{}, fmt={})",
                           handle.width, handle.height,
                           static_cast<uint32_t>(handle.format));
            // Don't reset the caller's handle -- they may still inspect it.
            // But we do clear the external reference so the pool owns it exclusively.
            handle.reset();
            return;
        }
    }

    // If not found in pool (shouldn't happen), just let it fall through.
    VEGA_LOG_WARN("TexturePool: released texture not found in pool, dropping");
    handle.reset();
}

void TexturePool::clear()
{
    size_t count = pool_.size();
    pool_.clear();
    VEGA_LOG_INFO("TexturePool: cleared {} textures", count);
}

size_t TexturePool::poolSize() const
{
    return pool_.size();
}

size_t TexturePool::freeCount() const
{
    size_t count = 0;
    for (const auto& entry : pool_) {
        if (!entry.in_use)
            ++count;
    }
    return count;
}

TexturePool::TextureHandle TexturePool::createTexture(uint32_t width, uint32_t height,
                                                       DXGI_FORMAT format)
{
    TextureHandle handle;
    handle.width = width;
    handle.height = height;
    handle.format = format;

    if (!device_) {
        VEGA_LOG_ERROR("TexturePool: createTexture called with null device");
        return handle;
    }

    // Create the 2D texture with both SRV and UAV bind flags
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width      = width;
    desc.Height     = height;
    desc.MipLevels  = 1;
    desc.ArraySize  = 1;
    desc.Format     = format;
    desc.SampleDesc = { 1, 0 };
    desc.Usage      = D3D11_USAGE_DEFAULT;
    desc.BindFlags  = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    HRESULT hr = device_->CreateTexture2D(&desc, nullptr, &handle.texture);
    if (FAILED(hr)) {
        VEGA_LOG_ERROR("TexturePool: CreateTexture2D failed: 0x{:08X}",
                       static_cast<uint32_t>(hr));
        handle.reset();
        return handle;
    }

    // Create SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format                    = format;
    srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels       = 1;

    hr = device_->CreateShaderResourceView(handle.texture.Get(), &srv_desc, &handle.srv);
    if (FAILED(hr)) {
        VEGA_LOG_ERROR("TexturePool: CreateSRV failed: 0x{:08X}",
                       static_cast<uint32_t>(hr));
        handle.reset();
        return handle;
    }

    // Create UAV
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format             = format;
    uav_desc.ViewDimension      = D3D11_UAV_DIMENSION_TEXTURE2D;
    uav_desc.Texture2D.MipSlice = 0;

    hr = device_->CreateUnorderedAccessView(handle.texture.Get(), &uav_desc, &handle.uav);
    if (FAILED(hr)) {
        VEGA_LOG_ERROR("TexturePool: CreateUAV failed: 0x{:08X}",
                       static_cast<uint32_t>(hr));
        handle.reset();
        return handle;
    }

    return handle;
}

bool TexturePool::matches(const PoolEntry& entry, uint32_t w, uint32_t h, DXGI_FORMAT fmt)
{
    return entry.handle.width  == w
        && entry.handle.height == h
        && entry.handle.format == fmt;
}

} // namespace vega
