#pragma once
#include "RawImage.h"
#include <filesystem>
#include <optional>

namespace vega
{

struct GpsCoord
{
    double latitude;
    double longitude;
    double altitude;
};

class ExifReader
{
public:
    /// Read EXIF tags from the file and populate the metadata struct.
    static void enrichMetadata(const std::filesystem::path& filepath,
                               RawImageMetadata& metadata);

    /// Extract GPS coordinates from EXIF data, if present.
    static std::optional<GpsCoord> readGps(const std::filesystem::path& filepath);

    /// Read a .xmp sidecar file alongside the RAW, returning its content.
    static std::optional<std::string> readXmpSidecar(const std::filesystem::path& filepath);
};

} // namespace vega
