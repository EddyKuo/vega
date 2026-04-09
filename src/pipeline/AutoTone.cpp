#include "pipeline/AutoTone.h"
#include <cmath>
#include <algorithm>

namespace vega {

AutoToneResult computeAutoTone(const uint8_t* rgba, uint32_t width, uint32_t height)
{
    AutoToneResult result;
    if (!rgba || width == 0 || height == 0) return result;

    // Build luminance histogram
    uint32_t hist[256] = {};
    uint32_t total = width * height;
    for (uint32_t i = 0; i < total; ++i) {
        float r = rgba[i * 4 + 0];
        float g = rgba[i * 4 + 1];
        float b = rgba[i * 4 + 2];
        int lum = static_cast<int>(0.2126f * r + 0.7152f * g + 0.0722f * b);
        lum = std::clamp(lum, 0, 255);
        hist[lum]++;
    }

    // CDF and percentiles
    auto findPercentile = [&](float pct) -> int {
        uint32_t target = static_cast<uint32_t>(total * pct);
        uint32_t sum = 0;
        for (int i = 0; i < 256; ++i) {
            sum += hist[i];
            if (sum >= target) return i;
        }
        return 255;
    };

    int p1  = findPercentile(0.01f);
    int p5  = findPercentile(0.05f);
    int p50 = findPercentile(0.50f);
    int p95 = findPercentile(0.95f);
    int p99 = findPercentile(0.99f);

    // Exposure: shift median toward target middle gray (0.46 in sRGB ~ 118/255)
    float median_norm = p50 / 255.0f;
    if (median_norm > 0.01f)
        result.exposure = std::clamp(std::log2(0.46f / median_norm), -3.0f, 3.0f);

    // Blacks: pull P1 toward ~5
    if (p1 > 10)
        result.blacks = std::clamp(-(p1 - 5) * 2.0f, -100.0f, 0.0f);

    // Whites: push P99 toward ~250
    if (p99 < 240)
        result.whites = std::clamp((250 - p99) * 2.0f, 0.0f, 100.0f);

    // Highlights: if bright end compressed
    if (p99 - p95 < 20 && p95 > 200)
        result.highlights = std::clamp(-(255.0f - p95) * 0.5f, -100.0f, 0.0f);

    // Shadows: if dark end compressed
    if (p5 - p1 < 20 && p5 < 50)
        result.shadows = std::clamp(p5 * 0.5f, 0.0f, 100.0f);

    // Contrast: based on dynamic range
    float range = static_cast<float>(p95 - p5);
    if (range < 180)
        result.contrast = std::clamp((200.0f - range) * 0.3f, -30.0f, 50.0f);

    return result;
}

} // namespace vega
