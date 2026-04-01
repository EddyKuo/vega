#pragma once
#include "raw/RawImage.h"
#include <vector>
#include <cstdint>

namespace vega {

class SimplePipeline {
public:
    // Convert RawImage (Bayer float data) to RGBA8 sRGB buffer
    // Output: width * height * 4 bytes (RGBA, 8-bit per channel)
    static std::vector<uint8_t> process(const RawImage& raw);

private:
    // Bilinear demosaic: Bayer -> RGB linear float
    static void demosaic(const float* bayer, float* rgb,
                         uint32_t width, uint32_t height, uint32_t bayer_pattern);

    // Apply white balance multipliers (normalize by green)
    static void whiteBalance(float* rgb, uint32_t pixel_count,
                             const float wb_mul[4]);

    // Camera RGB -> XYZ -> linear sRGB (3x3 matrix multiplications)
    static void colorTransform(float* rgb, uint32_t pixel_count,
                               const float cam_to_xyz[9]);

    // Linear -> sRGB gamma
    static float linearToSRGB(float x);

    // Convert float RGB [0,1] to RGBA8
    static void toRGBA8(const float* rgb, uint8_t* rgba, uint32_t pixel_count);
};

} // namespace vega
