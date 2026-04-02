#pragma once
// Fast math utilities for pixel processing hot paths.
// These trade precision for speed — acceptable for 8-bit output.

#include <cstdint>
#include <cmath>
#include <algorithm>
#include <array>

namespace vega {

// ── Fast exp approximation (Schraudolph, 1999) ──
// ~5% max error, 10x faster than std::exp
inline float fast_exp(float x)
{
    // Clamp to avoid overflow/underflow
    x = std::clamp(x, -80.0f, 80.0f);
    union { float f; int32_t i; } v;
    v.i = static_cast<int32_t>(12102203.0f * x + 1065353216.0f);
    return v.f;
}

// ── Fast pow(2, x) ──
inline float fast_exp2(float x)
{
    return fast_exp(x * 0.6931472f); // ln(2)
}

// ── sRGB gamma LUT (singleton, 65536 entries) ──
struct SRGBGammaLUT {
    static constexpr int SIZE = 65536;
    std::array<uint8_t, SIZE> table; // direct linear float -> sRGB uint8

    SRGBGammaLUT()
    {
        for (int i = 0; i < SIZE; ++i) {
            float x = static_cast<float>(i) / static_cast<float>(SIZE - 1);
            float g;
            if (x < 0.0031308f)
                g = 12.92f * x;
            else
                g = 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
            int v = static_cast<int>(g * 255.0f + 0.5f);
            table[i] = static_cast<uint8_t>(std::clamp(v, 0, 255));
        }
    }

    // Convert linear float [0,1] directly to sRGB uint8
    inline uint8_t operator()(float linear) const
    {
        if (linear <= 0.0f) return 0;
        if (linear >= 1.0f) return 255;
        return table[static_cast<int>(linear * (SIZE - 1) + 0.5f)];
    }
};

inline const SRGBGammaLUT& gammaLUT()
{
    static const SRGBGammaLUT lut;
    return lut;
}

// ── Forceinline for MSVC ──
#ifdef _MSC_VER
#define VEGA_FORCEINLINE __forceinline
#else
#define VEGA_FORCEINLINE inline __attribute__((always_inline))
#endif

// ── Inline pixel ops ──
VEGA_FORCEINLINE float vega_luminance(float r, float g, float b)
{
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

VEGA_FORCEINLINE float vega_clamp01(float x)
{
    return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x);
}

} // namespace vega
