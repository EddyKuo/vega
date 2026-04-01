#include "raw/RawDecoder.h"
#include "core/Logger.h"

#include <libraw/libraw.h>

#include <algorithm>
#include <cstring>
#include <memory>

namespace vega {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// Map a LibRaw error code to our RawDecodeError enum.
static RawDecodeError mapLibRawError(int rc) {
    switch (rc) {
        case LIBRAW_FILE_UNSUPPORTED:
            return RawDecodeError::UnsupportedFormat;
        case LIBRAW_INPUT_CLOSED:
        case LIBRAW_IO_ERROR:
            return RawDecodeError::FileNotFound;
        case LIBRAW_UNSUFFICIENT_MEMORY:
            return RawDecodeError::OutOfMemory;
        case LIBRAW_DATA_ERROR:
        case LIBRAW_UNSPECIFIED_ERROR:
        default:
            return RawDecodeError::LibRawError;
    }
}

/// Convenience: a unique_ptr-like RAII wrapper that calls recycle() on scope exit.
struct LibRawDeleter {
    void operator()(LibRaw* p) const {
        if (p) {
            p->recycle();
            delete p;
        }
    }
};
using LibRawPtr = std::unique_ptr<LibRaw, LibRawDeleter>;

/// Create a fresh LibRaw instance wrapped in our RAII pointer.
static LibRawPtr makeLibRaw() {
    return LibRawPtr(new (std::nothrow) LibRaw());
}

/// Populate a RawImageMetadata struct from an already-opened LibRaw instance.
static void fillMetadata(LibRaw& raw, RawImageMetadata& meta) {
    const auto& idata  = raw.imgdata.idata;
    const auto& other  = raw.imgdata.other;
    const auto& sizes  = raw.imgdata.sizes;
    const auto& lens   = raw.imgdata.lens;

    meta.camera_make  = idata.make  ? idata.make  : "";
    meta.camera_model = idata.model ? idata.model : "";

    meta.lens_model = lens.Lens[0] != '\0'
                          ? std::string(lens.Lens)
                          : std::string();

    meta.iso_speed      = static_cast<uint32_t>(other.iso_speed);
    meta.shutter_speed  = other.shutter;
    meta.aperture       = other.aperture;
    meta.focal_length_mm = other.focal_len;
    meta.orientation    = sizes.flip;     // LibRaw stores EXIF orientation here

    // Timestamp -> human-readable string
    if (other.timestamp != 0) {
        // Use a thread-safe approach: gmtime_s on MSVC, gmtime_r elsewhere.
        std::tm tm_buf{};
        const std::time_t ts = static_cast<std::time_t>(other.timestamp);
#ifdef _MSC_VER
        gmtime_s(&tm_buf, &ts);
#else
        gmtime_r(&ts, &tm_buf);
#endif
        char buf[32]{};
        std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm_buf);
        meta.datetime_original = buf;
    }

    // Bayer pattern – stored as a 4-char string in cdesc (e.g. "RGBG").
    // We encode it as a 32-bit value where each byte is one colour channel
    // character so callers can compare with known constants.
    meta.bayer_pattern = 0;
    if (idata.cdesc[0]) {
        meta.bayer_pattern =
            (static_cast<uint32_t>(static_cast<uint8_t>(idata.cdesc[0])) << 24) |
            (static_cast<uint32_t>(static_cast<uint8_t>(idata.cdesc[1])) << 16) |
            (static_cast<uint32_t>(static_cast<uint8_t>(idata.cdesc[2])) <<  8) |
            (static_cast<uint32_t>(static_cast<uint8_t>(idata.cdesc[3])));
    }
}

// ---------------------------------------------------------------------------
// RawDecoder::decode
// ---------------------------------------------------------------------------

Result<RawImage, RawDecodeError>
RawDecoder::decode(const std::filesystem::path& filepath) {

    if (!std::filesystem::exists(filepath)) {
        VEGA_LOG_ERROR("RawDecoder::decode – file not found: {}", filepath.string());
        return Result<RawImage, RawDecodeError>::err(RawDecodeError::FileNotFound);
    }

    auto lr = makeLibRaw();
    if (!lr) {
        VEGA_LOG_ERROR("RawDecoder::decode – out of memory allocating LibRaw");
        return Result<RawImage, RawDecodeError>::err(RawDecodeError::OutOfMemory);
    }

    // Open -----------------------------------------------------------------
    int rc = lr->open_file(filepath.string().c_str());
    if (rc != LIBRAW_SUCCESS) {
        VEGA_LOG_ERROR("RawDecoder::decode – open_file failed: {} ({})",
                       libraw_strerror(rc), rc);
        return Result<RawImage, RawDecodeError>::err(mapLibRawError(rc));
    }

    VEGA_LOG_INFO("RawDecoder::decode – opened {} ({}x{})",
                  filepath.filename().string(),
                  lr->imgdata.sizes.raw_width,
                  lr->imgdata.sizes.raw_height);

    // Unpack ---------------------------------------------------------------
    rc = lr->unpack();
    if (rc != LIBRAW_SUCCESS) {
        VEGA_LOG_ERROR("RawDecoder::decode – unpack failed: {} ({})",
                       libraw_strerror(rc), rc);
        return Result<RawImage, RawDecodeError>::err(mapLibRawError(rc));
    }

    // Collect sizes --------------------------------------------------------
    const uint32_t width  = lr->imgdata.sizes.raw_width;
    const uint32_t height = lr->imgdata.sizes.raw_height;

    if (width == 0 || height == 0) {
        VEGA_LOG_ERROR("RawDecoder::decode – zero-size image");
        return Result<RawImage, RawDecodeError>::err(RawDecodeError::CorruptFile);
    }

    // Determine raw data pointer.
    // LibRaw exposes either raw_image (Bayer, 16-bit) or color4_image (Foveon/4-colour).
    // We only support Bayer sensors for now.
    const auto* raw_pixels = lr->imgdata.rawdata.raw_image;
    if (!raw_pixels) {
        VEGA_LOG_ERROR("RawDecoder::decode – no Bayer data (Foveon / X-Trans not yet supported)");
        return Result<RawImage, RawDecodeError>::err(RawDecodeError::UnsupportedFormat);
    }

    // Black / white levels -------------------------------------------------
    const float black = static_cast<float>(lr->imgdata.color.black);
    const float white = static_cast<float>(lr->imgdata.color.maximum);
    const float range = (white - black > 1.0f) ? (white - black) : 1.0f;

    // Build the output RawImage -------------------------------------------
    RawImage img;
    img.width  = width;
    img.height = height;
    img.black_level = black;
    img.white_level = white;
    img.bits_per_sample = 14;  // Default; actual bit depth varies per camera

    // Normalise Bayer data to [0, 1] floats.
    const size_t total_pixels = static_cast<size_t>(width) * height;
    try {
        img.bayer_data.resize(total_pixels);
    } catch (const std::bad_alloc&) {
        VEGA_LOG_ERROR("RawDecoder::decode – out of memory allocating bayer_data ({} pixels)", total_pixels);
        return Result<RawImage, RawDecodeError>::err(RawDecodeError::OutOfMemory);
    }

    for (size_t i = 0; i < total_pixels; ++i) {
        float val = (static_cast<float>(raw_pixels[i]) - black) / range;
        img.bayer_data[i] = std::clamp(val, 0.0f, 1.0f);
    }

    // White-balance multipliers -------------------------------------------
    const float* cam_mul = lr->imgdata.color.cam_mul;
    for (int c = 0; c < 4; ++c) {
        img.wb_multipliers[c] = (cam_mul[c] > 0.0f) ? cam_mul[c] : 1.0f;
    }

    // Colour matrix: camera RGB -> XYZ (3x3) ------------------------------
    // LibRaw stores the inverse (XYZ -> camera) in rgb_cam, and the
    // forward matrix in cam_xyz.  We use cam_xyz here so callers can go
    // from camera space to XYZ directly.
    // cam_xyz is [4][3]; we only take the first 3 rows (RGB).
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            img.color_matrix[r * 3 + c] = lr->imgdata.color.cam_xyz[r][c];
        }
    }

    // Metadata -------------------------------------------------------------
    fillMetadata(*lr, img.metadata);

    VEGA_LOG_INFO("RawDecoder::decode – success: {}x{}, black={}, white={}, ISO={}",
                  width, height, black, white, img.metadata.iso_speed);

    return Result<RawImage, RawDecodeError>::ok(std::move(img));
}

// ---------------------------------------------------------------------------
// RawDecoder::readMetadata
// ---------------------------------------------------------------------------

Result<RawImageMetadata, RawDecodeError>
RawDecoder::readMetadata(const std::filesystem::path& filepath) {

    if (!std::filesystem::exists(filepath)) {
        VEGA_LOG_ERROR("RawDecoder::readMetadata – file not found: {}", filepath.string());
        return Result<RawImageMetadata, RawDecodeError>::err(RawDecodeError::FileNotFound);
    }

    auto lr = makeLibRaw();
    if (!lr) {
        VEGA_LOG_ERROR("RawDecoder::readMetadata – out of memory allocating LibRaw");
        return Result<RawImageMetadata, RawDecodeError>::err(RawDecodeError::OutOfMemory);
    }

    int rc = lr->open_file(filepath.string().c_str());
    if (rc != LIBRAW_SUCCESS) {
        VEGA_LOG_ERROR("RawDecoder::readMetadata – open_file failed: {} ({})",
                       libraw_strerror(rc), rc);
        return Result<RawImageMetadata, RawDecodeError>::err(mapLibRawError(rc));
    }

    RawImageMetadata meta;
    fillMetadata(*lr, meta);

    VEGA_LOG_INFO("RawDecoder::readMetadata – {} {} ({})",
                  meta.camera_make, meta.camera_model, filepath.filename().string());

    return Result<RawImageMetadata, RawDecodeError>::ok(std::move(meta));
}

// ---------------------------------------------------------------------------
// RawDecoder::extractThumbnail
// ---------------------------------------------------------------------------

Result<std::vector<uint8_t>, RawDecodeError>
RawDecoder::extractThumbnail(const std::filesystem::path& filepath) {

    if (!std::filesystem::exists(filepath)) {
        VEGA_LOG_ERROR("RawDecoder::extractThumbnail – file not found: {}", filepath.string());
        return Result<std::vector<uint8_t>, RawDecodeError>::err(RawDecodeError::FileNotFound);
    }

    auto lr = makeLibRaw();
    if (!lr) {
        VEGA_LOG_ERROR("RawDecoder::extractThumbnail – out of memory allocating LibRaw");
        return Result<std::vector<uint8_t>, RawDecodeError>::err(RawDecodeError::OutOfMemory);
    }

    int rc = lr->open_file(filepath.string().c_str());
    if (rc != LIBRAW_SUCCESS) {
        VEGA_LOG_ERROR("RawDecoder::extractThumbnail – open_file failed: {} ({})",
                       libraw_strerror(rc), rc);
        return Result<std::vector<uint8_t>, RawDecodeError>::err(mapLibRawError(rc));
    }

    rc = lr->unpack_thumb();
    if (rc != LIBRAW_SUCCESS) {
        VEGA_LOG_ERROR("RawDecoder::extractThumbnail – unpack_thumb failed: {} ({})",
                       libraw_strerror(rc), rc);
        return Result<std::vector<uint8_t>, RawDecodeError>::err(mapLibRawError(rc));
    }

    const auto& thumb = lr->imgdata.thumbnail;
    if (!thumb.thumb || thumb.tlength == 0) {
        VEGA_LOG_ERROR("RawDecoder::extractThumbnail – no thumbnail data present");
        return Result<std::vector<uint8_t>, RawDecodeError>::err(RawDecodeError::CorruptFile);
    }

    std::vector<uint8_t> data;
    try {
        data.resize(thumb.tlength);
    } catch (const std::bad_alloc&) {
        VEGA_LOG_ERROR("RawDecoder::extractThumbnail – out of memory ({} bytes)", thumb.tlength);
        return Result<std::vector<uint8_t>, RawDecodeError>::err(RawDecodeError::OutOfMemory);
    }

    std::memcpy(data.data(), thumb.thumb, thumb.tlength);

    VEGA_LOG_INFO("RawDecoder::extractThumbnail – {} bytes from {}",
                  thumb.tlength, filepath.filename().string());

    return Result<std::vector<uint8_t>, RawDecodeError>::ok(std::move(data));
}

} // namespace vega
