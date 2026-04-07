#define NOMINMAX
#include "catalog/ThumbnailCache.h"
#include "raw/RawDecoder.h"
#include "core/Logger.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")

#include <algorithm>
#include <fstream>
#include <chrono>

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

static std::string levelSuffix(ThumbnailCache::Level level)
{
    switch (level) {
        case ThumbnailCache::Level::Micro:  return "_160";
        case ThumbnailCache::Level::Small:  return "_320";
        case ThumbnailCache::Level::Medium: return "_1024";
        case ThumbnailCache::Level::Large:  return "_2048";
    }
    return "_320";
}

/// Attempt to create a D3D11 texture+SRV from raw JPEG bytes.
/// We use the embedded thumbnail from the RAW file which is already JPEG.
/// For display in ImGui we need an RGBA texture. We decode the JPEG
/// in-memory using a minimal approach: since the RAW decoder already gives
/// us JPEG bytes from the embedded thumbnail, we create a simple 1x1 placeholder
/// if we cannot decode, or use the raw bytes if the GPU supports JPEG decode.
///
/// For a real implementation we would use stb_image or WIC. Here we use
/// WIC (Windows Imaging Component) since we are on Windows with D3D11.
static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
createSRVFromJpeg(ID3D11Device* device, const std::vector<uint8_t>& jpeg_data)
{
    if (!device || jpeg_data.empty()) return nullptr;

    // Use WIC to decode JPEG to RGBA bitmap
    // We include wincodec.h for IWICImagingFactory
    Microsoft::WRL::ComPtr<IWICImagingFactory> wic_factory;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wic_factory));

    if (FAILED(hr)) {
        VEGA_LOG_ERROR("ThumbnailCache – CoCreateInstance(WICImagingFactory) failed: 0x{:08X}", static_cast<unsigned>(hr));
        return nullptr;
    }

    // Create stream from memory
    Microsoft::WRL::ComPtr<IWICStream> stream;
    hr = wic_factory->CreateStream(&stream);
    if (FAILED(hr)) return nullptr;

    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(jpeg_data.data()),
        static_cast<DWORD>(jpeg_data.size()));
    if (FAILED(hr)) return nullptr;

    // Create decoder
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = wic_factory->CreateDecoderFromStream(
        stream.Get(), nullptr,
        WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return nullptr;

    // Get first frame
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return nullptr;

    // Convert to RGBA 32bpp
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

    UINT width = 0, height = 0;
    converter->GetSize(&width, &height);

    if (width == 0 || height == 0) return nullptr;

    // Copy pixels
    std::vector<uint8_t> rgba(width * height * 4);
    hr = converter->CopyPixels(nullptr, width * 4,
                               static_cast<UINT>(rgba.size()), rgba.data());
    if (FAILED(hr)) return nullptr;

    // Create D3D11 texture
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

void ThumbnailCache::initialize(const std::filesystem::path& cache_dir,
                                 ID3D11Device* device)
{
    cache_dir_ = cache_dir;
    device_ = device;

    // Ensure cache directory exists
    std::error_code ec;
    std::filesystem::create_directories(cache_dir_, ec);
    if (ec) {
        VEGA_LOG_WARN("ThumbnailCache::initialize – failed to create cache dir: {}",
                      ec.message());
    }

    // COM should already be initialized by the application (STA for UI thread)

    VEGA_LOG_INFO("ThumbnailCache::initialize – cache dir: {}", cache_dir_.string());
}

ID3D11ShaderResourceView* ThumbnailCache::getThumbnail(
    const std::string& uuid,
    const std::filesystem::path& raw_path,
    Level level)
{
    std::string key = uuid + levelSuffix(level);

    // 1. Check memory cache
    auto it = memory_cache_.find(key);
    if (it != memory_cache_.end()) {
        it->second.last_access = currentTimestamp();
        return it->second.srv.Get();
    }

    // 2. Check disk cache
    auto disk_file = diskPath(uuid, level);
    if (std::filesystem::exists(disk_file)) {
        if (loadFromDisk(key, disk_file)) {
            return memory_cache_[key].srv.Get();
        }
    }

    // 3. Generate: extract thumbnail from RAW file
    auto thumb_result = RawDecoder::extractThumbnail(raw_path);
    if (!thumb_result) {
        VEGA_LOG_WARN("ThumbnailCache::getThumbnail – failed to extract thumbnail for {}",
                      raw_path.filename().string());
        return nullptr;
    }

    const auto& jpeg_data = thumb_result.value();

    // Save to disk for future use
    saveToDisk(jpeg_data, disk_file);

    // Decode to GPU texture
    auto srv = createSRVFromJpeg(device_, jpeg_data);
    if (!srv) {
        VEGA_LOG_WARN("ThumbnailCache::getThumbnail – failed to create SRV for {}",
                      raw_path.filename().string());
        return nullptr;
    }

    // Evict if at capacity
    if (memory_cache_.size() >= MAX_MEMORY_ENTRIES) {
        evictOldest();
    }

    CacheEntry entry;
    entry.srv = srv;
    entry.last_access = currentTimestamp();
    memory_cache_[key] = std::move(entry);

    return memory_cache_[key].srv.Get();
}

void ThumbnailCache::generateAsync(const std::string& uuid,
                                    const std::filesystem::path& raw_path)
{
    // For now, do synchronous generation of the Small level.
    // A full implementation would dispatch to a background thread.
    // We pre-generate the most commonly needed level.
    getThumbnail(uuid, raw_path, Level::Small);
}

void ThumbnailCache::clear()
{
    memory_cache_.clear();
    VEGA_LOG_INFO("ThumbnailCache::clear – memory cache cleared");
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

std::filesystem::path ThumbnailCache::diskPath(const std::string& uuid, Level level)
{
    // Store thumbnails in sub-directories based on first 2 chars of UUID
    // to avoid having too many files in one directory.
    std::string subdir = uuid.size() >= 2 ? uuid.substr(0, 2) : "00";
    auto dir = cache_dir_ / subdir;

    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    return dir / (uuid + levelSuffix(level) + ".jpg");
}

bool ThumbnailCache::loadFromDisk(const std::string& key,
                                   const std::filesystem::path& path)
{
    // Read JPEG from disk
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto size = file.tellg();
    if (size <= 0) return false;

    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) return false;

    // Decode to texture
    auto srv = createSRVFromJpeg(device_, data);
    if (!srv) return false;

    // Evict if needed
    if (memory_cache_.size() >= MAX_MEMORY_ENTRIES) {
        evictOldest();
    }

    CacheEntry entry;
    entry.srv = srv;
    entry.last_access = currentTimestamp();
    memory_cache_[key] = std::move(entry);

    return true;
}

bool ThumbnailCache::saveToDisk(const std::vector<uint8_t>& jpeg_data,
                                 const std::filesystem::path& path)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        VEGA_LOG_WARN("ThumbnailCache::saveToDisk – failed to open: {}", path.string());
        return false;
    }

    file.write(reinterpret_cast<const char*>(jpeg_data.data()),
               static_cast<std::streamsize>(jpeg_data.size()));
    return file.good();
}

void ThumbnailCache::evictOldest()
{
    if (memory_cache_.empty()) return;

    // Find the entry with the oldest last_access timestamp
    auto oldest = memory_cache_.begin();
    for (auto it = memory_cache_.begin(); it != memory_cache_.end(); ++it) {
        if (it->second.last_access < oldest->second.last_access) {
            oldest = it;
        }
    }

    memory_cache_.erase(oldest);
}

} // namespace vega
