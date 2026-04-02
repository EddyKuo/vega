#define NOMINMAX
#include "pipeline/cpu/HSLNode.h"
#include "pipeline/cpu/FastMath.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace vega {

// Fast RGB to HSL (no std::fmod, minimal branching)
VEGA_FORCEINLINE void fastRGBtoHSL(float r, float g, float b,
                                    float& h, float& s, float& l)
{
    float mx = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
    float mn = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
    float d = mx - mn;

    l = (mx + mn) * 0.5f;

    if (d < 1e-6f) {
        h = 0.0f;
        s = 0.0f;
        return;
    }

    s = (l <= 0.5f) ? (d / (mx + mn + 1e-10f))
                     : (d / (2.0f - mx - mn + 1e-10f));

    if (mx == r) {
        h = 60.0f * ((g - b) / d);
        if (h < 0.0f) h += 360.0f;
    } else if (mx == g) {
        h = 60.0f * ((b - r) / d) + 120.0f;
    } else {
        h = 60.0f * ((r - g) / d) + 240.0f;
    }
}

// Fast HSL to RGB
VEGA_FORCEINLINE void fastHSLtoRGB(float h, float s, float l,
                                    float& r, float& g, float& b)
{
    if (s < 1e-6f) {
        r = g = b = l;
        return;
    }

    float q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
    float p = 2.0f * l - q;
    float h_norm = h * (1.0f / 360.0f);

    auto hue2rgb = [](float p, float q, float t) -> float {
        if (t < 0.0f) t += 1.0f;
        if (t > 1.0f) t -= 1.0f;
        if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
        if (t < 0.5f) return q;
        if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
        return p;
    };

    r = hue2rgb(p, q, h_norm + 1.0f / 3.0f);
    g = hue2rgb(p, q, h_norm);
    b = hue2rgb(p, q, h_norm - 1.0f / 3.0f);
}

// Pre-computed cosine LUT for channel weights (avoids per-pixel std::cos)
static constexpr int COS_LUT_SIZE = 1024;
static float s_cos_lut[COS_LUT_SIZE + 1];
static bool s_cos_lut_ready = false;

static void initCosLUT()
{
    if (s_cos_lut_ready) return;
    for (int i = 0; i <= COS_LUT_SIZE; ++i) {
        float t = static_cast<float>(i) / COS_LUT_SIZE; // 0 to 1 -> 0 to PI
        s_cos_lut[i] = std::cos(t * 3.14159265f);
    }
    s_cos_lut_ready = true;
}

// Fast cosine via LUT, input in [0, PI]
VEGA_FORCEINLINE float fastCos01(float t)
{
    // t is ratio diff/halfWidth, already in [0, 1]
    int idx = static_cast<int>(t * COS_LUT_SIZE + 0.5f);
    if (idx < 0) idx = 0;
    if (idx > COS_LUT_SIZE) idx = COS_LUT_SIZE;
    return s_cos_lut[idx];
}

void HSLNode::process(Tile& tile, const EditRecipe& recipe)
{
    // Check if any adjustments are non-zero
    bool hasHSL = false;
    for (int i = 0; i < 8; ++i) {
        if (recipe.hsl_hue[i] != 0.0f || recipe.hsl_saturation[i] != 0.0f ||
            recipe.hsl_luminance[i] != 0.0f) {
            hasHSL = true;
            break;
        }
    }
    bool hasVibrance = std::abs(recipe.vibrance) > 0.01f;
    bool hasSaturation = std::abs(recipe.saturation) > 0.01f;

    if (!hasHSL && !hasVibrance && !hasSaturation) {
        VEGA_LOG_DEBUG("HSLNode: no adjustments, skipping");
        return;
    }

    initCosLUT();

    // Pre-compute channel centers and half-widths
    static constexpr float centers[8] = {0, 30, 60, 120, 180, 240, 270, 300};
    static constexpr float halfWidths[8] = {30, 20, 30, 40, 40, 30, 25, 30};

    // Pre-compute inverse half-widths for fast division
    float invHW[8];
    for (int i = 0; i < 8; ++i)
        invHW[i] = 1.0f / halfWidths[i];

    const float vib_scale = recipe.vibrance / 100.0f;
    const float sat_mul = 1.0f + recipe.saturation / 100.0f;

    const uint32_t rows = tile.height;
    const uint32_t cols = tile.width;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    for (uint32_t row = 0; row < rows; ++row) {
        float* __restrict rowPtr = tile.data + row * stride;
        for (uint32_t col = 0; col < cols; ++col) {
            float* __restrict px = rowPtr + col * ch;

            float r = px[0], g = px[1], b = px[2];
            float h, s, l;
            fastRGBtoHSL(vega_clamp01(r), vega_clamp01(g), vega_clamp01(b), h, s, l);

            // Per-channel HSL
            if (hasHSL) {
                float hueShift = 0, satShift = 0, lumShift = 0;
                float totalW = 0;

                for (int i = 0; i < 8; ++i) {
                    float diff = h - centers[i];
                    if (diff > 180.0f) diff -= 360.0f;
                    else if (diff < -180.0f) diff += 360.0f;
                    float absDiff = (diff < 0) ? -diff : diff;

                    if (absDiff >= halfWidths[i]) continue;

                    float w = 0.5f * (1.0f + fastCos01(absDiff * invHW[i]));
                    totalW += w;
                    hueShift += w * recipe.hsl_hue[i];
                    satShift += w * recipe.hsl_saturation[i];
                    lumShift += w * recipe.hsl_luminance[i];
                }

                if (totalW > 1e-6f) {
                    float inv = 1.0f / totalW;
                    hueShift *= inv;
                    satShift *= inv;
                    lumShift *= inv;
                }

                h += hueShift;
                if (h < 0.0f) h += 360.0f;
                else if (h >= 360.0f) h -= 360.0f;

                s *= (1.0f + satShift * 0.01f);
                if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;

                l += lumShift * 0.005f;
                if (l < 0.0f) l = 0.0f; else if (l > 1.0f) l = 1.0f;
            }

            // Vibrance
            if (hasVibrance) {
                float prot = 1.0f - s;
                prot *= prot;
                s *= (1.0f + vib_scale * prot);
                if (s > 1.0f) s = 1.0f;
            }

            // Global saturation
            if (hasSaturation) {
                s *= sat_mul;
                if (s < 0.0f) s = 0.0f; else if (s > 1.0f) s = 1.0f;
            }

            fastHSLtoRGB(h, s, l, r, g, b);
            px[0] = r;
            px[1] = g;
            px[2] = b;
        }
    }
}

} // namespace vega
