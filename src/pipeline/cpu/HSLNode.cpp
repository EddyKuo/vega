#include "pipeline/cpu/HSLNode.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>

namespace vega {

// PI constant
static constexpr float kPi = 3.14159265358979323846f;

void HSLNode::rgbToHSL(float r, float g, float b, float& h, float& s, float& l)
{
    float maxC = std::max({r, g, b});
    float minC = std::min({r, g, b});
    float delta = maxC - minC;

    l = (maxC + minC) * 0.5f;

    if (delta < 1e-6f) {
        h = 0.0f;
        s = 0.0f;
        return;
    }

    s = (l <= 0.5f) ? (delta / (maxC + minC))
                     : (delta / (2.0f - maxC - minC));

    if (maxC == r) {
        h = 60.0f * std::fmod((g - b) / delta + 6.0f, 6.0f);
    } else if (maxC == g) {
        h = 60.0f * ((b - r) / delta + 2.0f);
    } else {
        h = 60.0f * ((r - g) / delta + 4.0f);
    }

    if (h < 0.0f) h += 360.0f;
    if (h >= 360.0f) h -= 360.0f;
}

static float hueToChannel(float p, float q, float t)
{
    if (t < 0.0f) t += 1.0f;
    if (t > 1.0f) t -= 1.0f;
    if (t < 1.0f / 6.0f) return p + (q - p) * 6.0f * t;
    if (t < 1.0f / 2.0f) return q;
    if (t < 2.0f / 3.0f) return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

void HSLNode::hslToRGB(float h, float s, float l, float& r, float& g, float& b)
{
    if (s < 1e-6f) {
        r = g = b = l;
        return;
    }

    // Normalize hue to [0,1]
    float hNorm = h / 360.0f;
    hNorm = hNorm - std::floor(hNorm); // wrap

    float q = (l < 0.5f) ? (l * (1.0f + s)) : (l + s - l * s);
    float p = 2.0f * l - q;

    r = hueToChannel(p, q, hNorm + 1.0f / 3.0f);
    g = hueToChannel(p, q, hNorm);
    b = hueToChannel(p, q, hNorm - 1.0f / 3.0f);
}

void HSLNode::computeChannelWeights(float hue_deg, std::array<float, 8>& weights)
{
    // Channel center hues in degrees
    static constexpr float centers[8] = {
        0.0f,    // Red
        30.0f,   // Orange
        60.0f,   // Yellow
        120.0f,  // Green
        180.0f,  // Aqua
        240.0f,  // Blue
        270.0f,  // Purple
        300.0f   // Magenta
    };

    // Half-widths: defines the angular distance at which blending reaches zero.
    // Adjacent channels share a cosine blend in their overlap region.
    static constexpr float halfWidths[8] = {
        30.0f,  // Red (spans roughly 330-30)
        30.0f,  // Orange (spans roughly 0-60)
        30.0f,  // Yellow (spans roughly 30-90)
        60.0f,  // Green (spans roughly 60-180)
        60.0f,  // Aqua (spans roughly 120-240)
        30.0f,  // Blue (spans roughly 210-270)
        30.0f,  // Purple (spans roughly 240-300)
        30.0f   // Magenta (spans roughly 270-330)
    };

    float totalWeight = 0.0f;
    for (int i = 0; i < 8; ++i) {
        // Angular distance, wrapping around 360
        float diff = hue_deg - centers[i];
        // Wrap to [-180, 180]
        if (diff > 180.0f) diff -= 360.0f;
        if (diff < -180.0f) diff += 360.0f;
        diff = std::abs(diff);

        float hw = halfWidths[i];
        if (diff >= hw) {
            weights[i] = 0.0f;
        } else {
            // Cosine blend: 1 at center, 0 at half-width
            weights[i] = 0.5f * (1.0f + std::cos(kPi * diff / hw));
        }
        totalWeight += weights[i];
    }

    // Normalize weights so they sum to 1 (in case of overlap)
    if (totalWeight > 1e-6f) {
        float inv = 1.0f / totalWeight;
        for (int i = 0; i < 8; ++i)
            weights[i] *= inv;
    }
}

void HSLNode::process(Tile& tile, const EditRecipe& recipe)
{
    // Check if any HSL adjustments are non-zero
    bool hasHSL = false;
    for (int i = 0; i < 8; ++i) {
        if (std::abs(recipe.hsl_hue[i]) > 0.01f ||
            std::abs(recipe.hsl_saturation[i]) > 0.01f ||
            std::abs(recipe.hsl_luminance[i]) > 0.01f) {
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

    VEGA_LOG_DEBUG("HSLNode: applying HSL={} vibrance={} saturation={}",
                   hasHSL, recipe.vibrance, recipe.saturation);

    const uint32_t rows = tile.height;
    const uint32_t cols = tile.width;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    for (uint32_t row = 0; row < rows; ++row) {
        float* rowPtr = tile.data + row * stride;
        for (uint32_t col = 0; col < cols; ++col) {
            float* px = rowPtr + col * ch;

            float r = std::clamp(px[0], 0.0f, 1.0f);
            float g = std::clamp(px[1], 0.0f, 1.0f);
            float b = std::clamp(px[2], 0.0f, 1.0f);

            float h, s, l;
            rgbToHSL(r, g, b, h, s, l);

            // Per-channel HSL adjustments
            if (hasHSL) {
                std::array<float, 8> weights;
                computeChannelWeights(h, weights);

                float hueShift = 0.0f;
                float satShift = 0.0f;
                float lumShift = 0.0f;

                for (int i = 0; i < 8; ++i) {
                    if (weights[i] > 1e-6f) {
                        hueShift += weights[i] * recipe.hsl_hue[i];
                        satShift += weights[i] * recipe.hsl_saturation[i];
                        lumShift += weights[i] * recipe.hsl_luminance[i];
                    }
                }

                // Hue shift (in degrees, scale from slider units to degrees)
                h += hueShift;
                if (h < 0.0f) h += 360.0f;
                if (h >= 360.0f) h -= 360.0f;

                // Saturation shift: slider [-100,100] maps to multiplier
                float satMul = 1.0f + satShift / 100.0f;
                s = std::clamp(s * satMul, 0.0f, 1.0f);

                // Luminance shift: slider [-100,100]
                l = std::clamp(l + lumShift / 200.0f, 0.0f, 1.0f);
            }

            // Global vibrance: boosts desaturated colors more, protects already saturated
            if (hasVibrance) {
                float vibranceScale = recipe.vibrance / 100.0f;
                // Protection factor: less boost for already-saturated colors
                float protection = 1.0f - s; // [0,1]: 0=fully saturated, 1=desaturated
                protection = protection * protection; // quadratic: stronger protection
                float vibMul = 1.0f + vibranceScale * protection;
                s = std::clamp(s * vibMul, 0.0f, 1.0f);
            }

            // Global saturation
            if (hasSaturation) {
                float satMul = 1.0f + recipe.saturation / 100.0f;
                s = std::clamp(s * satMul, 0.0f, 1.0f);
            }

            // Convert back to RGB
            hslToRGB(h, s, l, r, g, b);

            px[0] = std::clamp(r, 0.0f, 1.0f);
            px[1] = std::clamp(g, 0.0f, 1.0f);
            px[2] = std::clamp(b, 0.0f, 1.0f);
        }
    }
}

} // namespace vega
