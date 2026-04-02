#include "pipeline/cpu/ExposureNode.h"
#include "pipeline/cpu/FastMath.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>

namespace vega {

void ExposureNode::process(Tile& tile, const EditRecipe& recipe)
{
    const float exposure = recipe.exposure;
    const float contrast = recipe.contrast;
    const float highlights = recipe.highlights;
    const float shadows = recipe.shadows;
    const float whites = recipe.whites;
    const float blacks = recipe.blacks;

    VEGA_LOG_DEBUG("ExposureNode: ev={} contrast={} highlights={} shadows={} whites={} blacks={}",
                   exposure, contrast, highlights, shadows, whites, blacks);

    // Pre-compute constants
    const float exp_mul = std::pow(2.0f, exposure);
    const bool do_contrast = std::abs(contrast) > 0.01f;
    const bool do_highlights = std::abs(highlights) > 0.01f;
    const bool do_shadows = std::abs(shadows) > 0.01f;
    const bool do_whites = std::abs(whites) > 0.01f;
    const bool do_blacks = std::abs(blacks) > 0.01f;

    // Pre-compute contrast sigmoid constants (avoid per-pixel exp)
    float sig_k = 0, sig_offset = 0, sig_scale = 1;
    bool contrast_positive = false;
    float contrast_lerp = 1.0f;
    if (do_contrast) {
        float s = contrast / 100.0f;
        if (s > 0.0f) {
            contrast_positive = true;
            sig_k = (1.0f + s * 4.0f) * 6.0f; // combined k*6
            float sig0 = 1.0f / (1.0f + fast_exp(sig_k * 0.5f));
            float sig1 = 1.0f / (1.0f + fast_exp(-sig_k * 0.5f));
            sig_offset = sig0;
            sig_scale = 1.0f / (sig1 - sig0);
        } else {
            contrast_lerp = 1.0f + s; // [0, 1]
        }
    }

    const float hl_amount = highlights / 100.0f;
    const float sh_amount = shadows / 100.0f;

    const uint32_t rows = tile.height;
    const uint32_t cols = tile.width;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    for (uint32_t row = 0; row < rows; ++row) {
        float* __restrict rowPtr = tile.data + row * stride;
        for (uint32_t col = 0; col < cols; ++col) {
            float* __restrict px = rowPtr + col * ch;
            float r = px[0] * exp_mul;
            float g = px[1] * exp_mul;
            float b = px[2] * exp_mul;

            // Highlights/Shadows with inlined luminance
            if (do_highlights || do_shadows) {
                float lum = vega_clamp01(0.2126f * r + 0.7152f * g + 0.0722f * b);
                float mul = 1.0f;

                if (do_highlights) {
                    float mask = vega_clamp01((lum - 0.25f) * (1.0f / 0.75f));
                    mask *= mask;
                    mul *= 1.0f - hl_amount * mask * 0.5f;
                }
                if (do_shadows) {
                    float mask = vega_clamp01(1.0f - lum * 2.0f);
                    mask *= mask;
                    mul *= 1.0f + sh_amount * mask * 0.5f;
                }

                r *= mul;
                g *= mul;
                b *= mul;
            }

            // Whites: boost/cut the brightest values (lum > 0.7)
            if (do_whites) {
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                float mask = vega_clamp01((lum - 0.7f) * (1.0f / 0.3f));
                mask *= mask;
                float adj = 1.0f + (whites / 100.0f) * mask * 0.6f;
                r *= adj; g *= adj; b *= adj;
            }

            // Blacks: lift/crush the darkest values (lum < 0.3)
            if (do_blacks) {
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                float mask = vega_clamp01(1.0f - lum * (1.0f / 0.3f));
                mask *= mask;
                float adj = 1.0f + (blacks / 100.0f) * mask * 0.6f;
                r *= adj; g *= adj; b *= adj;
            }

            // Contrast
            if (do_contrast) {
                if (contrast_positive) {
                    // Sigmoid via fast_exp
                    r = vega_clamp01(r);
                    g = vega_clamp01(g);
                    b = vega_clamp01(b);
                    r = (1.0f / (1.0f + fast_exp(-sig_k * (r - 0.5f))) - sig_offset) * sig_scale;
                    g = (1.0f / (1.0f + fast_exp(-sig_k * (g - 0.5f))) - sig_offset) * sig_scale;
                    b = (1.0f / (1.0f + fast_exp(-sig_k * (b - 0.5f))) - sig_offset) * sig_scale;
                } else {
                    r = 0.5f + (r - 0.5f) * contrast_lerp;
                    g = 0.5f + (g - 0.5f) * contrast_lerp;
                    b = 0.5f + (b - 0.5f) * contrast_lerp;
                }
            }

            px[0] = r;
            px[1] = g;
            px[2] = b;
        }
    }
}

} // namespace vega
