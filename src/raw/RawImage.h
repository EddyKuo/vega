#pragma once
#include <cstdint>
#include <vector>
#include <string>

namespace vega
{

struct RawImageMetadata
{
    std::string camera_make;
    std::string camera_model;
    std::string lens_model;
    uint32_t    iso_speed        = 0;
    float       shutter_speed    = 0.0f;  // seconds
    float       aperture         = 0.0f;  // f-number
    float       focal_length_mm  = 0.0f;
    int         orientation      = 1;     // EXIF orientation tag
    std::string datetime_original;
    uint32_t    bayer_pattern    = 0;     // RGGB, BGGR, GRBG, GBRG
};

struct RawImage
{
    std::vector<float> bayer_data;           // single channel, W x H
    uint32_t width  = 0;
    uint32_t height = 0;
    float wb_multipliers[4] = {1, 1, 1, 1}; // R, G1, B, G2
    float color_matrix[9]   = {};            // Camera -> XYZ (3x3)
    RawImageMetadata metadata;
    uint16_t bits_per_sample = 14;
    float black_level = 0.0f;
    float white_level = 16383.0f;
};

} // namespace vega
