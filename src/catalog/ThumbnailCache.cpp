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
        case 5:
        case 8:  return WICBitmapTransformRotate270;
        case 6:  return WICBitmapTransformRotate90;
        case 2:  return WICBitmapTransformFlipHorizontal;
        case 4:  return WICBitmapTransformFlipVertical;
        case 7:  return static_cast<WICBitmapTransformOptions>(
                     WICBitmapTransformRotate90 | WICBitmapTransformFlipHorizontal);
        default: return WICBitmapTransformRotate0;
    }
}

/// Decode JPEG bytes to RGBA pixels (CPU side, thread-safe).
/// Returns empty vector on failure.
static std::vector<uint8_t> decodeJpegToRGBA(
    const std::vector<uint8_t>& jpeg_data, int orientation,
    uint32_t& out_width, uint32_t& out_height)
{
    out_width = out_height = 0;
    if (jpeg_data.empty()) return {};

    Microsoft::WRL::ComPtr<IWICImagingFactory> wic;
    HRESULT hr = CoCreateInstance(
        CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&wic));
    if (FAILED(hr)) return {};

    Microsoft::WRL::ComPtr<IWICStream> stream;
    hr = wic->CreateStream(&stream);
    if (FAILED(hr)) return {};

    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(jpeg_data.data()),
        static_cast<DWORD>(jpeg_data.size()));
    if (FAILED(hr)) return {};

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    hr = wic->CreateDecoderFromStream(
        stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) return {};

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return {};

    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    hr = wic->CreateFormatConverter(&converter);
    if (FAILED(hr)) return {};

    hr = converter->Initialize(
        frame.Get(), GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeMedianCut);
    if (FAILED(hr)) return {};

    // Apply orientation
    IWICBitmapSource* final_source = converter.Get();
    Microsoft::WRL::ComPtr<IWICBitmapFlipRotator> rotator;
    WICBitmapTransformOptions xform = orientationToWicTransform(orientation);

    if (xform != WICBitmapTransformRotate0) {
        hr = wic->CreateBitmapFlipRotator(&rotator);
        if (SUCCEEDED(hr)) {
            hr = rotator->Initialize(converter.Get(), xform);
            if (SUCCEEDED(hr))
                final_source = rotator.Get();
        }
    }

    UINT w = 0, h = 0;
    final_source->GetSize(&w, &h);
    if (w == 0 || h == 0) return {};

    std::vector<uint8_t> rgba(w * h * 4);
    hr = final_source->CopyPixels(nullptr, w * 4,
                                  static_cast<UINT>(rgba.size()), rgba.data());
    if (FAILED(hr)) return {};

    out_width = w;
    out_height = h;
    return rgba;
}

/// Create D3D11 SRV from RGBA pixels (must be called with valid device).
static Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
createSRVFromRGBA(ID3D11Device* device, const uint8_t* rgba,
                  uint32_t width, uint32_t height)
{
    if (!device || !rgba || width == 0 || height == 0) return nullptr;

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc = {1, 0};
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init = {};
    init.pSysMem = rgba;
    init.SysMemPitch = width * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device->CreateTexture2D(&desc, &init, &tex);
    if (FAILED(hr)) return nullptr;

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    hr = device->CreateShaderResourceView(tex.Get(), nullptr, &srv);
    if (FAILED(hr)) return nullptr;

    return srv;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

ThumbnailCache::~ThumbnailCache()
{
    shutdown();
}

void ThumbnailCache::initialize(Database* db, ID3D11Device* device)
{
    db_ = db;
    device_ = device;

    // Start worker threads (one per CPU core, min 2, max 8)
    unsigned int cores = std::thread::hardware_concurrency();
    unsigned int num_workers = std::clamp(cores, 2u, 8u);

    stop_workers_ = false;
    for (unsigned int i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&ThumbnailCache::workerLoop, this);
    }

    VEGA_LOG_INFO("ThumbnailCache: initialized with {} worker threads ({} cores detected)",
                  num_workers, cores);
}

void ThumbnailCache::shutdown()
{
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        stop_workers_ = true;
    }
    queue_cv_.notify_all();

    for (auto& t : workers_) {
        if (t.joinable()) t.join();
    }
    workers_.clear();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

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

    // 2. Not in cache -- queue for background decoding (if not already queued)
    {
        std::lock_guard<std::mutex> lock(queued_mutex_);
        if (queued_keys_.count(key) == 0) {
            queued_keys_.insert(key);

            DecodeRequest req;
            req.key = key;
            req.uuid = uuid;
            req.raw_path = raw_path;
            req.level = level;
            req.orientation = orientation;

            {
                std::lock_guard<std::mutex> qlock(queue_mutex_);
                work_queue_.push(std::move(req));
            }
            queue_cv_.notify_one();
        }
    }

    return nullptr; // not ready yet, will appear next frame via flushPending
}

void ThumbnailCache::flushPending()
{
    std::vector<DecodeResult> results;
    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        results.swap(pending_results_);
    }

    for (auto& r : results) {
        if (r.rgba.empty()) continue;

        auto srv = createSRVFromRGBA(device_, r.rgba.data(), r.width, r.height);
        if (!srv) continue;

        if (memory_cache_.size() >= MAX_MEMORY_ENTRIES) evictOldest();

        CacheEntry entry;
        entry.srv = srv;
        entry.last_access = currentTimestamp();
        memory_cache_[r.key] = std::move(entry);

        // Allow re-queue if evicted later
        std::lock_guard<std::mutex> lock(queued_mutex_);
        queued_keys_.erase(r.key);
    }
}

void ThumbnailCache::clear()
{
    // Drain work queue
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        std::queue<DecodeRequest> empty;
        work_queue_.swap(empty);
    }
    {
        std::lock_guard<std::mutex> lock(queued_mutex_);
        queued_keys_.clear();
    }
    {
        std::lock_guard<std::mutex> lock(result_mutex_);
        pending_results_.clear();
    }

    memory_cache_.clear();
    VEGA_LOG_INFO("ThumbnailCache: cleared");
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

void ThumbnailCache::workerLoop()
{
    // Each thread needs its own COM initialization for WIC
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    while (true) {
        DecodeRequest req;

        // Wait for work
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return stop_workers_ || !work_queue_.empty();
            });

            if (stop_workers_ && work_queue_.empty()) break;

            req = std::move(work_queue_.front());
            work_queue_.pop();
        }

        // Try to load JPEG data: first from DB, then extract from RAW
        std::vector<uint8_t> jpeg_data;

        if (db_) {
            jpeg_data = db_->loadThumbnail(req.uuid, static_cast<int>(req.level));
        }

        if (jpeg_data.empty()) {
            auto result = RawDecoder::extractThumbnail(req.raw_path);
            if (result) {
                jpeg_data = std::move(result.value());

                // Save to DB for next time
                if (db_ && !jpeg_data.empty()) {
                    db_->saveThumbnail(req.uuid, static_cast<int>(req.level),
                                       jpeg_data.data(), jpeg_data.size());
                }
            }
        }

        if (jpeg_data.empty()) {
            // Failed — remove from queued set so it can be retried later
            std::lock_guard<std::mutex> lock(queued_mutex_);
            queued_keys_.erase(req.key);
            continue;
        }

        // Decode JPEG to RGBA pixels (CPU work, thread-safe)
        uint32_t w = 0, h = 0;
        auto rgba = decodeJpegToRGBA(jpeg_data, req.orientation, w, h);

        if (!rgba.empty()) {
            DecodeResult dr;
            dr.key = req.key;
            dr.rgba = std::move(rgba);
            dr.width = w;
            dr.height = h;

            std::lock_guard<std::mutex> lock(result_mutex_);
            pending_results_.push_back(std::move(dr));
        } else {
            std::lock_guard<std::mutex> lock(queued_mutex_);
            queued_keys_.erase(req.key);
        }
    }

    CoUninitialize();
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void ThumbnailCache::evictOldest()
{
    if (memory_cache_.empty()) return;

    auto oldest = memory_cache_.begin();
    for (auto it = memory_cache_.begin(); it != memory_cache_.end(); ++it) {
        if (it->second.last_access < oldest->second.last_access)
            oldest = it;
    }

    memory_cache_.erase(oldest);
}

} // namespace vega
