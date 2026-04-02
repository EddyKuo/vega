#pragma once
#include <filesystem>
#include <functional>
#include <cstdint>
#include <vector>
#include <string>

namespace vega {

struct ExportSettings {
    enum class Format { JPEG, TIFF_8, TIFF_16, PNG_8, PNG_16 };
    Format format = Format::JPEG;
    int jpeg_quality = 92;

    enum class ResizeMode { Original, LongEdge, ShortEdge, Percentage };
    ResizeMode resize_mode = ResizeMode::Original;
    uint32_t resize_value = 0;  // pixels or percentage

    bool embed_icc_profile = true;
    bool preserve_exif = true;
    bool strip_gps = false;

    std::filesystem::path output_dir;
    std::string filename_template = "{original}";  // {original}, {date}, {camera}
};

class ExportManager {
public:
    using ProgressCallback = std::function<void(float progress, const std::string& status)>;

    // Export a single image
    bool exportImage(const std::vector<uint8_t>& rgba_data,
                     uint32_t width, uint32_t height,
                     const ExportSettings& settings,
                     const std::filesystem::path& output_path);

    // Export with progress (for batch)
    void exportAsync(const std::vector<uint8_t>& rgba_data,
                     uint32_t width, uint32_t height,
                     const ExportSettings& settings,
                     const std::filesystem::path& output_path,
                     ProgressCallback callback);

private:
    bool exportJPEG(const uint8_t* data, uint32_t w, uint32_t h, int quality,
                    const std::filesystem::path& path);
    bool exportPNG(const uint8_t* data, uint32_t w, uint32_t h,
                   const std::filesystem::path& path);
    bool exportTIFF(const uint8_t* data, uint32_t w, uint32_t h, int bits,
                    const std::filesystem::path& path);

    std::vector<uint8_t> resize(const uint8_t* data, uint32_t w, uint32_t h,
                                uint32_t new_w, uint32_t new_h);
};

} // namespace vega
