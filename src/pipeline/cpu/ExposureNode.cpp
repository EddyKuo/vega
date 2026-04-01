#include "pipeline/cpu/ExposureNode.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>

namespace vega {

float ExposureNode::luminance(float r, float g, float b)
{
    // Rec. 709 luminance coefficients
    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

float ExposureNode::contrastCurve(float x, float strength)
{
    // S-curve using a smooth sigmoid-like function centered at 0.5.
    // strength in [-100, 100]: 0 = no change, positive = more contrast.
    if (std::abs(strength) < 0.01f)
        return x;

    // Map strength [-100,100] to a curve parameter
    // Using a simple polynomial S-curve: x' = 0.5 + (x - 0.5) * gain
    // where gain varies with distance from midpoint for an S-shape.
    float s = strength / 100.0f; // [-1, 1]
    float centered = x - 0.5f;

    // Use a power-based S-curve
    // For positive contrast, steepen around midpoint; for negative, flatten
    float t = std::clamp(x, 0.0f, 1.0f);
    if (s > 0.0f) {
        // Increase contrast: apply sigmoidal curve
        float k = 1.0f + s * 4.0f; // contrast multiplier [1, 5]
        float sig = 1.0f / (1.0f + std::exp(-k * (t - 0.5f) * 6.0f));
        // Normalize so 0->0 and 1->1
        float sig0 = 1.0f / (1.0f + std::exp(-k * (-0.5f) * 6.0f));
        float sig1 = 1.0f / (1.0f + std::exp(-k * (0.5f) * 6.0f));
        return (sig - sig0) / (sig1 - sig0);
    } else {
        // Decrease contrast: lerp toward flat line (0.5)
        float factor = 1.0f + s; // [0, 1] where s is negative
        return 0.5f + centered * factor;
    }
}

float ExposureNode::highlightsCurve(float lum, float amount)
{
    // amount in [-100, 100]
    // Affects bright areas (lum > 0.5). Negative = recover highlights (darken brights).
    // Returns a multiplier to apply to the pixel.
    if (std::abs(amount) < 0.01f)
        return 1.0f;

    // Soft mask: smoothly ramp from 0 at lum=0.25 to 1 at lum=1.0
    float mask = std::clamp((lum - 0.25f) / 0.75f, 0.0f, 1.0f);
    mask = mask * mask; // smooth

    // Scale: amount/100 * mask * range. Negative reduces brightness.
    float shift = -(amount / 100.0f) * mask * 0.5f;
    return 1.0f + shift;
}

float ExposureNode::shadowsCurve(float lum, float amount)
{
    // amount in [-100, 100]
    // Positive = lift shadows (brighten darks).
    // Returns a multiplier to apply to the pixel.
    if (std::abs(amount) < 0.01f)
        return 1.0f;

    // Soft mask: smoothly ramp from 1 at lum=0 to 0 at lum=0.5
    float mask = std::clamp(1.0f - lum / 0.5f, 0.0f, 1.0f);
    mask = mask * mask; // smooth

    // Scale: amount/100 * mask
    float shift = (amount / 100.0f) * mask * 0.5f;
    return 1.0f + shift;
}

void ExposureNode::process(Tile& tile, const EditRecipe& recipe)
{
    const float exposure = recipe.exposure;
    const float contrast = recipe.contrast;
    const float highlights = recipe.highlights;
    const float shadows = recipe.shadows;

    // Exposure multiplier: 2^ev
    const float exp_mul = std::pow(2.0f, exposure);

    const bool hasContrast = std::abs(contrast) > 0.01f;
    const bool hasHighlights = std::abs(highlights) > 0.01f;
    const bool hasShadows = std::abs(shadows) > 0.01f;

    VEGA_LOG_DEBUG("ExposureNode: ev={} contrast={} highlights={} shadows={}",
                   exposure, contrast, highlights, shadows);

    const uint32_t rows = tile.height;
    const uint32_t cols = tile.width;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    for (uint32_t row = 0; row < rows; ++row) {
        float* rowPtr = tile.data + row * stride;
        for (uint32_t col = 0; col < cols; ++col) {
            float* px = rowPtr + col * ch;

            // Apply exposure
            px[0] *= exp_mul;
            px[1] *= exp_mul;
            px[2] *= exp_mul;

            // Highlights and shadows (luminance-based)
            if (hasHighlights || hasShadows) {
                float lum = luminance(px[0], px[1], px[2]);
                lum = std::clamp(lum, 0.0f, 1.0f);

                float mul = 1.0f;
                if (hasHighlights)
                    mul *= highlightsCurve(lum, highlights);
                if (hasShadows)
                    mul *= shadowsCurve(lum, shadows);

                px[0] *= mul;
                px[1] *= mul;
                px[2] *= mul;
            }

            // Contrast (S-curve, applied in [0,1] range)
            if (hasContrast) {
                px[0] = contrastCurve(std::clamp(px[0], 0.0f, 1.0f), contrast);
                px[1] = contrastCurve(std::clamp(px[1], 0.0f, 1.0f), contrast);
                px[2] = contrastCurve(std::clamp(px[2], 0.0f, 1.0f), contrast);
            }
        }
    }
}

} // namespace vega
