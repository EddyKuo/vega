#include "raw/ExifReader.h"
#include "core/Logger.h"

#include <exiv2/exiv2.hpp>
#include <fstream>
#include <sstream>
#include <cmath>

namespace vega
{

// ---------------------------------------------------------------------------
// Helper: safely read a string tag from the ExifData map.
// ---------------------------------------------------------------------------
static std::string readStringTag(const Exiv2::ExifData& exif,
                                 const std::string& key)
{
    auto it = exif.findKey(Exiv2::ExifKey(key));
    if (it == exif.end())
        return {};
    return it->toString();
}

// ---------------------------------------------------------------------------
// Helper: safely read a long (integer) tag.
// ---------------------------------------------------------------------------
static std::optional<long> readLongTag(const Exiv2::ExifData& exif,
                                       const std::string& key)
{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4244) // possible loss of data in Exiv2 internals
#endif
    auto it = exif.findKey(Exiv2::ExifKey(key));
    if (it == exif.end())
        return std::nullopt;
    return it->toInt64();
#ifdef _MSC_VER
#pragma warning(pop)
#endif
}

// ---------------------------------------------------------------------------
// Helper: safely read a float (Rational) tag.
// ---------------------------------------------------------------------------
static std::optional<float> readFloatTag(const Exiv2::ExifData& exif,
                                         const std::string& key)
{
    auto it = exif.findKey(Exiv2::ExifKey(key));
    if (it == exif.end())
        return std::nullopt;
    return it->toFloat();
}

// ---------------------------------------------------------------------------
// Helper: convert EXIF GPS DMS rational values to decimal degrees.
//   GPS coordinates are stored as three Rationals: degrees, minutes, seconds.
// ---------------------------------------------------------------------------
static std::optional<double> dmsToDecimal(const Exiv2::ExifData& exif,
                                          const std::string& coordKey,
                                          const std::string& refKey)
{
    auto coordIt = exif.findKey(Exiv2::ExifKey(coordKey));
    auto refIt   = exif.findKey(Exiv2::ExifKey(refKey));
    if (coordIt == exif.end() || refIt == exif.end())
        return std::nullopt;

    // The coordinate value has 3 components: degrees, minutes, seconds
    if (coordIt->count() < 3)
        return std::nullopt;

    double degrees = coordIt->toFloat(0);
    double minutes = coordIt->toFloat(1);
    double seconds = coordIt->toFloat(2);

    double decimal = degrees + minutes / 60.0 + seconds / 3600.0;

    std::string ref = refIt->toString();
    if (ref == "S" || ref == "W")
        decimal = -decimal;

    return decimal;
}

// ---------------------------------------------------------------------------
// ExifReader::enrichMetadata
// ---------------------------------------------------------------------------
void ExifReader::enrichMetadata(const std::filesystem::path& filepath,
                                RawImageMetadata& metadata)
{
    try
    {
        auto image = Exiv2::ImageFactory::open(filepath.string());
        if (!image)
        {
            VEGA_LOG_WARN("ExifReader: failed to open '{}'", filepath.string());
            return;
        }
        image->readMetadata();

        const Exiv2::ExifData& exif = image->exifData();
        if (exif.empty())
        {
            VEGA_LOG_WARN("ExifReader: no EXIF data in '{}'", filepath.string());
            return;
        }

        // Camera make
        {
            auto val = readStringTag(exif, "Exif.Image.Make");
            if (!val.empty())
                metadata.camera_make = val;
            else
                VEGA_LOG_WARN("ExifReader: missing Exif.Image.Make in '{}'",
                              filepath.string());
        }

        // Camera model
        {
            auto val = readStringTag(exif, "Exif.Image.Model");
            if (!val.empty())
                metadata.camera_model = val;
            else
                VEGA_LOG_WARN("ExifReader: missing Exif.Image.Model in '{}'",
                              filepath.string());
        }

        // Lens model
        {
            auto val = readStringTag(exif, "Exif.Photo.LensModel");
            if (!val.empty())
                metadata.lens_model = val;
            else
                VEGA_LOG_WARN("ExifReader: missing Exif.Photo.LensModel in '{}'",
                              filepath.string());
        }

        // ISO speed
        {
            auto val = readLongTag(exif, "Exif.Photo.ISOSpeedRatings");
            if (val.has_value())
                metadata.iso_speed = static_cast<uint32_t>(val.value());
            else
                VEGA_LOG_WARN("ExifReader: missing Exif.Photo.ISOSpeedRatings in '{}'",
                              filepath.string());
        }

        // Shutter speed (exposure time in seconds)
        {
            auto val = readFloatTag(exif, "Exif.Photo.ExposureTime");
            if (val.has_value())
                metadata.shutter_speed = val.value();
            else
                VEGA_LOG_WARN("ExifReader: missing Exif.Photo.ExposureTime in '{}'",
                              filepath.string());
        }

        // Aperture (f-number)
        {
            auto val = readFloatTag(exif, "Exif.Photo.FNumber");
            if (val.has_value())
                metadata.aperture = val.value();
            else
                VEGA_LOG_WARN("ExifReader: missing Exif.Photo.FNumber in '{}'",
                              filepath.string());
        }

        // Focal length (mm)
        {
            auto val = readFloatTag(exif, "Exif.Photo.FocalLength");
            if (val.has_value())
                metadata.focal_length_mm = val.value();
            else
                VEGA_LOG_WARN("ExifReader: missing Exif.Photo.FocalLength in '{}'",
                              filepath.string());
        }

        // Orientation
        {
            auto val = readLongTag(exif, "Exif.Image.Orientation");
            if (val.has_value())
                metadata.orientation = static_cast<int>(val.value());
            else
                VEGA_LOG_WARN("ExifReader: missing Exif.Image.Orientation in '{}'",
                              filepath.string());
        }

        // Date/time original
        {
            auto val = readStringTag(exif, "Exif.Photo.DateTimeOriginal");
            if (!val.empty())
                metadata.datetime_original = val;
            else
                VEGA_LOG_WARN("ExifReader: missing Exif.Photo.DateTimeOriginal in '{}'",
                              filepath.string());
        }
    }
    catch (const Exiv2::Error& e)
    {
        VEGA_LOG_ERROR("ExifReader: exiv2 exception reading '{}': {}",
                       filepath.string(), e.what());
    }
    catch (const std::exception& e)
    {
        VEGA_LOG_ERROR("ExifReader: exception reading '{}': {}",
                       filepath.string(), e.what());
    }
}

// ---------------------------------------------------------------------------
// ExifReader::readGps
// ---------------------------------------------------------------------------
std::optional<GpsCoord> ExifReader::readGps(const std::filesystem::path& filepath)
{
    try
    {
        auto image = Exiv2::ImageFactory::open(filepath.string());
        if (!image)
        {
            VEGA_LOG_WARN("ExifReader: failed to open '{}' for GPS",
                          filepath.string());
            return std::nullopt;
        }
        image->readMetadata();

        const Exiv2::ExifData& exif = image->exifData();
        if (exif.empty())
            return std::nullopt;

        // Latitude
        auto latitude = dmsToDecimal(exif,
                                     "Exif.GPSInfo.GPSLatitude",
                                     "Exif.GPSInfo.GPSLatitudeRef");
        if (!latitude.has_value())
            return std::nullopt;

        // Longitude
        auto longitude = dmsToDecimal(exif,
                                      "Exif.GPSInfo.GPSLongitude",
                                      "Exif.GPSInfo.GPSLongitudeRef");
        if (!longitude.has_value())
            return std::nullopt;

        // Altitude (optional — default to 0)
        double altitude = 0.0;
        {
            auto altIt = exif.findKey(
                Exiv2::ExifKey("Exif.GPSInfo.GPSAltitude"));
            if (altIt != exif.end())
            {
                altitude = altIt->toFloat();

                // Check altitude ref: 0 = above sea level, 1 = below
                auto refIt = exif.findKey(
                    Exiv2::ExifKey("Exif.GPSInfo.GPSAltitudeRef"));
                if (refIt != exif.end() && refIt->toInt64() == 1)
                    altitude = -altitude;
            }
        }

        return GpsCoord{latitude.value(), longitude.value(), altitude};
    }
    catch (const Exiv2::Error& e)
    {
        VEGA_LOG_WARN("ExifReader: exiv2 exception reading GPS from '{}': {}",
                      filepath.string(), e.what());
    }
    catch (const std::exception& e)
    {
        VEGA_LOG_WARN("ExifReader: exception reading GPS from '{}': {}",
                      filepath.string(), e.what());
    }
    return std::nullopt;
}

// ---------------------------------------------------------------------------
// ExifReader::readXmpSidecar
// ---------------------------------------------------------------------------
std::optional<std::string> ExifReader::readXmpSidecar(
    const std::filesystem::path& filepath)
{
    try
    {
        // Look for a .xmp file with the same stem next to the RAW file.
        std::filesystem::path xmpPath = filepath;
        xmpPath.replace_extension(".xmp");

        if (!std::filesystem::exists(xmpPath))
        {
            // Also try uppercase extension (.XMP) — common on some systems.
            xmpPath.replace_extension(".XMP");
            if (!std::filesystem::exists(xmpPath))
                return std::nullopt;
        }

        std::ifstream in(xmpPath, std::ios::in | std::ios::binary);
        if (!in.is_open())
        {
            VEGA_LOG_WARN("ExifReader: could not open XMP sidecar '{}'",
                          xmpPath.string());
            return std::nullopt;
        }

        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    catch (const std::exception& e)
    {
        VEGA_LOG_WARN("ExifReader: exception reading XMP sidecar for '{}': {}",
                      filepath.string(), e.what());
    }
    return std::nullopt;
}

} // namespace vega
