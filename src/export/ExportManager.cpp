#define NOMINMAX
#include "export/ExportManager.h"
#include "core/Logger.h"

#include <jpeglib.h>
#include <png.h>
#include <tiffio.h>

#include <algorithm>
#include <cstring>
#include <thread>

namespace vega {

// ── RGBA to RGB strip (JPEG/PNG 8-bit need RGB, not RGBA) ──
static std::vector<uint8_t> rgbaToRgb(const uint8_t* rgba, uint32_t w, uint32_t h)
{
    std::vector<uint8_t> rgb(static_cast<size_t>(w) * h * 3);
    const size_t pixel_count = static_cast<size_t>(w) * h;
    for (size_t i = 0; i < pixel_count; ++i)
    {
        rgb[i * 3 + 0] = rgba[i * 4 + 0];
        rgb[i * 3 + 1] = rgba[i * 4 + 1];
        rgb[i * 3 + 2] = rgba[i * 4 + 2];
    }
    return rgb;
}

// ── Bilinear resize for RGBA8 data ──
std::vector<uint8_t> ExportManager::resize(const uint8_t* data, uint32_t w, uint32_t h,
                                           uint32_t new_w, uint32_t new_h)
{
    std::vector<uint8_t> out(static_cast<size_t>(new_w) * new_h * 4);

    float x_ratio = static_cast<float>(w - 1) / static_cast<float>(new_w);
    float y_ratio = static_cast<float>(h - 1) / static_cast<float>(new_h);

    for (uint32_t y = 0; y < new_h; ++y)
    {
        float gy = y * y_ratio;
        uint32_t y0 = static_cast<uint32_t>(gy);
        uint32_t y1 = std::min(y0 + 1, h - 1);
        float fy = gy - y0;

        for (uint32_t x = 0; x < new_w; ++x)
        {
            float gx = x * x_ratio;
            uint32_t x0 = static_cast<uint32_t>(gx);
            uint32_t x1 = std::min(x0 + 1, w - 1);
            float fx = gx - x0;

            for (int c = 0; c < 4; ++c)
            {
                float top_left     = data[(y0 * w + x0) * 4 + c];
                float top_right    = data[(y0 * w + x1) * 4 + c];
                float bottom_left  = data[(y1 * w + x0) * 4 + c];
                float bottom_right = data[(y1 * w + x1) * 4 + c];

                float top    = top_left + fx * (top_right - top_left);
                float bottom = bottom_left + fx * (bottom_right - bottom_left);
                float value  = top + fy * (bottom - top);

                out[(y * new_w + x) * 4 + c] = static_cast<uint8_t>(std::clamp(value + 0.5f, 0.0f, 255.0f));
            }
        }
    }

    return out;
}

// ── Compute output dimensions from resize settings ──
static void computeResizedDimensions(uint32_t w, uint32_t h,
                                     const ExportSettings& settings,
                                     uint32_t& out_w, uint32_t& out_h)
{
    out_w = w;
    out_h = h;

    if (settings.resize_mode == ExportSettings::ResizeMode::Original || settings.resize_value == 0)
        return;

    switch (settings.resize_mode)
    {
    case ExportSettings::ResizeMode::LongEdge:
    {
        uint32_t long_edge = std::max(w, h);
        if (settings.resize_value < long_edge)
        {
            float scale = static_cast<float>(settings.resize_value) / static_cast<float>(long_edge);
            out_w = static_cast<uint32_t>(w * scale + 0.5f);
            out_h = static_cast<uint32_t>(h * scale + 0.5f);
        }
        break;
    }
    case ExportSettings::ResizeMode::ShortEdge:
    {
        uint32_t short_edge = std::min(w, h);
        if (settings.resize_value < short_edge)
        {
            float scale = static_cast<float>(settings.resize_value) / static_cast<float>(short_edge);
            out_w = static_cast<uint32_t>(w * scale + 0.5f);
            out_h = static_cast<uint32_t>(h * scale + 0.5f);
        }
        break;
    }
    case ExportSettings::ResizeMode::Percentage:
    {
        float pct = static_cast<float>(settings.resize_value) / 100.0f;
        out_w = static_cast<uint32_t>(w * pct + 0.5f);
        out_h = static_cast<uint32_t>(h * pct + 0.5f);
        break;
    }
    default:
        break;
    }

    // Ensure at least 1x1
    out_w = std::max(out_w, 1u);
    out_h = std::max(out_h, 1u);
}

// ── JPEG export using libjpeg-turbo ──
bool ExportManager::exportJPEG(const uint8_t* data, uint32_t w, uint32_t h,
                               int quality, const std::filesystem::path& path)
{
    auto rgb = rgbaToRgb(data, w, h);

    FILE* fp = nullptr;
#ifdef _WIN32
    _wfopen_s(&fp, path.wstring().c_str(), L"wb");
#else
    fp = fopen(path.string().c_str(), "wb");
#endif
    if (!fp)
    {
        VEGA_LOG_ERROR("ExportJPEG: cannot open file {}", path.string());
        return false;
    }

    struct jpeg_compress_struct cinfo = {};
    struct jpeg_error_mgr jerr = {};
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = w;
    cinfo.image_height = h;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;

    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, quality, TRUE);

    jpeg_start_compress(&cinfo, TRUE);

    uint32_t row_stride = w * 3;
    while (cinfo.next_scanline < cinfo.image_height)
    {
        JSAMPROW row_pointer = const_cast<JSAMPROW>(&rgb[cinfo.next_scanline * row_stride]);
        jpeg_write_scanlines(&cinfo, &row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    fclose(fp);

    VEGA_LOG_INFO("Exported JPEG: {} ({}x{}, q={})", path.string(), w, h, quality);
    return true;
}

// ── PNG export using libpng ──
bool ExportManager::exportPNG(const uint8_t* data, uint32_t w, uint32_t h,
                              const std::filesystem::path& path)
{
    auto rgb = rgbaToRgb(data, w, h);

    FILE* fp = nullptr;
#ifdef _WIN32
    _wfopen_s(&fp, path.wstring().c_str(), L"wb");
#else
    fp = fopen(path.string().c_str(), "wb");
#endif
    if (!fp)
    {
        VEGA_LOG_ERROR("ExportPNG: cannot open file {}", path.string());
        return false;
    }

    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING,
                                                   nullptr, nullptr, nullptr);
    if (!png_ptr)
    {
        fclose(fp);
        return false;
    }

    png_infop info_ptr = png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        png_destroy_write_struct(&png_ptr, nullptr);
        fclose(fp);
        return false;
    }

    if (setjmp(png_jmpbuf(png_ptr)))
    {
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(fp);
        VEGA_LOG_ERROR("ExportPNG: libpng error");
        return false;
    }

    png_init_io(png_ptr, fp);

    png_set_IHDR(png_ptr, info_ptr, w, h, 8,
                 PNG_COLOR_TYPE_RGB, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

    png_write_info(png_ptr, info_ptr);

    uint32_t row_stride = w * 3;
    for (uint32_t y = 0; y < h; ++y)
    {
        png_bytep row = const_cast<png_bytep>(&rgb[y * row_stride]);
        png_write_row(png_ptr, row);
    }

    png_write_end(png_ptr, nullptr);
    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    VEGA_LOG_INFO("Exported PNG: {} ({}x{})", path.string(), w, h);
    return true;
}

// ── TIFF export using libtiff ──
bool ExportManager::exportTIFF(const uint8_t* data, uint32_t w, uint32_t h,
                               int bits, const std::filesystem::path& path)
{
#ifdef _WIN32
    TIFF* tif = TIFFOpenW(path.wstring().c_str(), "w");
#else
    TIFF* tif = TIFFOpen(path.string().c_str(), "w");
#endif
    if (!tif)
    {
        VEGA_LOG_ERROR("ExportTIFF: cannot open file {}", path.string());
        return false;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, w);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, h);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits);
    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, w * 3 * (bits / 8)));

    if (bits == 8)
    {
        // Convert RGBA8 to RGB8
        auto rgb = rgbaToRgb(data, w, h);
        uint32_t row_bytes = w * 3;
        for (uint32_t y = 0; y < h; ++y)
        {
            if (TIFFWriteScanline(tif, &rgb[y * row_bytes], y, 0) < 0)
            {
                TIFFClose(tif);
                return false;
            }
        }
    }
    else if (bits == 16)
    {
        // Upscale 8-bit RGBA to 16-bit RGB
        std::vector<uint16_t> row16(w * 3);
        for (uint32_t y = 0; y < h; ++y)
        {
            for (uint32_t x = 0; x < w; ++x)
            {
                size_t src = (static_cast<size_t>(y) * w + x) * 4;
                size_t dst = static_cast<size_t>(x) * 3;
                // Scale 0-255 to 0-65535: val * 257
                row16[dst + 0] = static_cast<uint16_t>(data[src + 0]) * 257;
                row16[dst + 1] = static_cast<uint16_t>(data[src + 1]) * 257;
                row16[dst + 2] = static_cast<uint16_t>(data[src + 2]) * 257;
            }
            if (TIFFWriteScanline(tif, row16.data(), y, 0) < 0)
            {
                TIFFClose(tif);
                return false;
            }
        }
    }

    TIFFClose(tif);
    VEGA_LOG_INFO("Exported TIFF: {} ({}x{}, {}-bit)", path.string(), w, h, bits);
    return true;
}

// ── Main export entry point ──
bool ExportManager::exportImage(const std::vector<uint8_t>& rgba_data,
                                uint32_t width, uint32_t height,
                                const ExportSettings& settings,
                                const std::filesystem::path& output_path)
{
    if (rgba_data.empty() || width == 0 || height == 0)
    {
        VEGA_LOG_ERROR("Export: invalid image data");
        return false;
    }

    // Ensure output directory exists
    auto parent_dir = output_path.parent_path();
    if (!parent_dir.empty())
    {
        std::error_code ec;
        std::filesystem::create_directories(parent_dir, ec);
        if (ec)
        {
            VEGA_LOG_ERROR("Export: cannot create directory {}: {}", parent_dir.string(), ec.message());
            return false;
        }
    }

    const char* fmt_names[] = {"JPEG", "TIFF8", "TIFF16", "PNG8", "PNG16"};
    VEGA_LOG_INFO("Export: {} {}x{} -> {}", fmt_names[static_cast<int>(settings.format)],
        width, height, output_path.string());

    // Resize if needed
    uint32_t out_w, out_h;
    computeResizedDimensions(width, height, settings, out_w, out_h);

    const uint8_t* export_data = rgba_data.data();
    std::vector<uint8_t> resized_buf;

    if (out_w != width || out_h != height)
    {
        resized_buf = resize(rgba_data.data(), width, height, out_w, out_h);
        export_data = resized_buf.data();
    }

    // Export based on format
    switch (settings.format)
    {
    case ExportSettings::Format::JPEG:
        return exportJPEG(export_data, out_w, out_h, settings.jpeg_quality, output_path);

    case ExportSettings::Format::PNG_8:
    case ExportSettings::Format::PNG_16:
        // PNG_16 would ideally use 16-bit pipeline data; for now export as 8-bit PNG
        return exportPNG(export_data, out_w, out_h, output_path);

    case ExportSettings::Format::TIFF_8:
        return exportTIFF(export_data, out_w, out_h, 8, output_path);

    case ExportSettings::Format::TIFF_16:
        return exportTIFF(export_data, out_w, out_h, 16, output_path);
    }

    return false;
}

// ── Async export ──
void ExportManager::exportAsync(const std::vector<uint8_t>& rgba_data,
                                uint32_t width, uint32_t height,
                                const ExportSettings& settings,
                                const std::filesystem::path& output_path,
                                ProgressCallback callback)
{
    // Copy data for the thread (the caller's buffer may change)
    auto data_copy = std::make_shared<std::vector<uint8_t>>(rgba_data);

    std::jthread([this, data_copy, width, height, settings, output_path, callback]()
    {
        if (callback) callback(0.0f, "Starting export...");

        if (callback) callback(0.3f, "Processing...");

        bool ok = exportImage(*data_copy, width, height, settings, output_path);

        if (callback)
        {
            if (ok)
                callback(1.0f, "Export complete: " + output_path.filename().string());
            else
                callback(-1.0f, "Export failed");
        }
    }).detach();
}

} // namespace vega
