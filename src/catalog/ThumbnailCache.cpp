#define NOMINMAX
#include "catalog/ThumbnailCache.h"
#include "catalog/Database.h"
#include "raw/RawDecoder.h"
#include "core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

#include <algorithm>
#include <chrono>
#include <filesystem>

namespace vega {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static uint64_t currentTimestamp()
{
    auto now = std::chrono::steady_clock::now();
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count());
}

static std::string cacheKey(const std::string& uuid, ThumbnailCache::Level level)
{
    const char* suffix = "_320";
    switch (level) {
        case ThumbnailCache::Level::Micro:  suffix = "_160"; break;
        case ThumbnailCache::Level::Small:  suffix = "_320"; break;
        case ThumbnailCache::Level::Medium: suffix = "_1024"; break;
        case ThumbnailCache::Level::Large:  suffix = "_2048"; break;
    }
    return uuid + suffix;
}

/// Map LibRaw flip / EXIF orientation to WIC transform flags.
static WICBitmapTransformOptions orientationToWicTransform(int orientation)
{
    switch (orientation) {
        case 3:  return WICBitmapTransformRotate180;
        case 5:  // LibRaw 90 CCW
        case 8:  // EXIF 90 CCW
            return WICBitmapTransformRotate270;
        case 6:  return WICBitmapTransformRotate90;
        case 2:  return WICBitmapTransformFlipHorizontal;
        case 4:  return WICBitmapTransformFlipVertical;
        case 7:  return static_cast<WICBitmapTransformOptions>(
                     WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
        default: return WICBitmapTransformRotate0;
    }
}

/// Decode JPEG bytes to a D3D11 SRV, applying EXIF orientation rotation.
static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
createSRVFromJpeg(ID3D11Device* device, const std::vector<uint8_t>& jpeg_data,
                  int orientation = 0)
{
    if (!device || jpeg_data.empty()) return nullptr;

    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wic_factory));

    if (FAILED(hr)) {
        VEGA_LOG_ERROR("ThumbnailCache: WIC factory creation failed: 0x{:08X}",
                       static_cast<unsigned>(hr));
        return nullptr;
    }

    Microsoft::WRL::ComPtr<IWICStream> stream;
    hr = wic_factory->CreateStream(&stream);
    if (FAILED(hr)) return nullptr;

    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(jpeg_data.data()),
        static_cast<DWORD>(jpeg_data.size()));
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = wic_factory->CreateDecoderFromStream(
        stream.Get(), nullptr,
        WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = wic_factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return nullptr;

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr, 0.0,
        WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)) return nullptr;

    // Apply orientation rotation if needed
    IWICBitmapSource* final_source = converter.Get();
    Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> rotator;
    WICBitmapTransformOptions xform = orientationToWicTransform(orientation);

    if (xform != WICBitmapTransformRotate0) {
        hr = wic_factory->CreateBitmapFlipRotator(&rotator);
        if (SUCCEEDED(hr)) {
            hr = rotator->Initialize(converter.Get(), xform);
            if (SUCCEEDED(hr)) {
                final_source = rotator.Get();
            }
        }
    }

    UINT width = 0, height = 0;
    final_source->GetSize(&width, &height);
    if (width == 0 || height == 0) return nullptr;

    std::vector<uint8_t> rgba(width * height * 4);
    hr = final_source->CopyPixels(nullptr, width * 4,
                                  static_cast<UINT>(rgba.size()), rgba.data());
    if (FAILED(hr)) return nullptr;

    D3D11_TEXTURE2D_DESC tex_desc = {};
    tex_desc.Width = width;
    tex_desc.Height = height;
    tex_desc.MipLevels = 1;
    tex_desc.ArraySize = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc = {1, 0};
    tex_desc.Usage = D3D11_USAGE_IMMUTABLE;
    tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init_data = {};
    init_data.pSysMem = rgba.data();
    init_data.SysMemPitch = width * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    hr = device->CreateTexture2D(&tex_desc, &init_data, &tex);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    hr = device->CreateShaderResourceView(tex.Get(), nullptr, &srv);
    if (FAILED(hr)) return nullptr;

    return srv;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void ThumbnailCache::initialize(Database* db, ID3D11Device* device)
{
    db_ = db;
    device_ = device;
    VEGA_LOG_INFO("ThumbnailCache: initialized (DB-backed)");
}

ID3D11ShaderResourceView* ThumbnailCache::getThumbnail(
    const std::string& uuid,
    const std::filesystem::path& raw_path,
    Level level,
    int orientation)
{
    std::string key = cacheKey(uuid, level);

    // 1. Check memory cache
    auto it = memory_cache_.find(key);
    if (it != memory_cache_.end()) {
        it->second.last_access = currentTimestamp();
        return it->second.srv.Get();
    }

    // 2. Check database
    if (db_) {
        auto jpeg_data = db_->loadThumbnail(uuid, static_cast<int>(level));
        if (!jpeg_data.empty()) {
            auto srv = createSRVFromJpeg(device_, jpeg_data, orientation);
            if (srv) {
                if (memory_cache_.size() >= MAX_MEMORY_ENTRIES) evictOldest();
                CacheEntry entry;
                entry.srv = srv;
                entry.last_access = currentTimestamp();
                memory_cache_[key] = std::move(entry);
                return memory_cache_[key].srv.Get();
            }
        }
    }

    // 3. Extract thumbnail from RAW file
    auto thumb_result = RawDecoder::extractThumbnail(raw_path);
    if (!thumb_result) {
        VEGA_LOG_WARN("ThumbnailCache: failed to extract thumbnail for {}",
                      raw_path.filename().string());
        return nullptr;
    }

    const auto& jpeg_data = thumb_result.value();

    // Save to database
    if (db_) {
        db_->saveThumbnail(uuid, static_cast<int>(level),
                           jpeg_data.data(), jpeg_data.size());
    }

    // Decode to GPU texture with orientation
    auto srv = createSRVFromJpeg(device_, jpeg_data, orientation);
    if (!srv) {
        VEGA_LOG_WARN("ThumbnailCache: failed to create SRV for {}",
                      raw_path.filename().string());
        return nullptr;
    }

    if (memory_cache_.size() >= MAX_MEMORY_ENTRIES) evictOldest();

    CacheEntry entry;
    entry.srv = srv;
    entry.last_access = currentTimestamp();
    memory_cache_[key] = std::move(entry);

    return memory_cache_[key].srv.Get();
}

void ThumbnailCache::clear()
{
    memory_cache_.clear();
    VEGA_LOG_INFO("ThumbnailCache: memory cache cleared");
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void ThumbnailCache::evictOldest()
{
    if (memory_cache_.empty()) return;

    auto oldest = memory_cache_.begin();
    for (auto it = memory_cache_.begin(); it != memory_cache_.end(); ++it) {
        if (it->second.last_access < oldest->second.last_access) {
            oldest = it;
        }
    }

    memory_cache_.erase(oldest);
}

} // namespace vega
