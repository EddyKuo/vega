#define NOMINMAX
#include "pipeline/cpu/ExposureNode.h"
#include "pipeline/cpu/FastMath.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>

namespace vega {

void ExposureNode::process(Tile& tile, const EditRecipe& recipe)
{
    const float ev = recipe.exposure;
    const float contrast = recipe.contrast;
    const float highlights = recipe.highlights;
    const float shadows = recipe.shadows;
    const float whites = recipe.whites;
    const float blacks = recipe.blacks;

    VEGA_LOG_DEBUG("ExposureNode: ev={} contrast={} hl={} sh={} wh={} bk={}",
                   ev, contrast, highlights, shadows, whites, blacks);

    const float exp_mul = std::pow(2.0f, ev);

    // Pre-compute flags
    const bool do_hl = std::abs(highlights) > 0.5f;
    const bool do_sh = std::abs(shadows) > 0.5f;
    const bool do_wh = std::abs(whites) > 0.5f;
    const bool do_bk = std::abs(blacks) > 0.5f;
    const bool do_tone = do_hl || do_sh || do_wh || do_bk;
    const bool do_contrast = std::abs(contrast) > 0.5f;

    // Tone adjustments as normalized values
    const float hl_str = highlights / 100.0f;
    const float sh_str = shadows / 100.0f;
    const float wh_str = whites / 100.0f;
    const float bk_str = blacks / 100.0f;

    // Contrast pre-compute
    float sig_k = 0, sig_offset = 0, sig_scale = 1;
    bool contrast_pos = false;
    float contrast_lerp = 1.0f;
    if (do_contrast) {
        float s = contrast / 100.0f;
        if (s > 0.0f) {
            contrast_pos = true;
            sig_k = (1.0f + s * 2.0f) * 4.0f;
            float sig0 = 1.0f / (1.0f + fast_exp(sig_k * 0.5f));
            float sig1 = 1.0f / (1.0f + fast_exp(-sig_k * 0.5f));
            sig_offset = sig0;
            sig_scale = 1.0f / (sig1 - sig0);
        } else {
            contrast_lerp = 1.0f + s;
        }
    }

    const uint32_t rows = tile.height;
    const uint32_t cols = tile.width;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    for (uint32_t row = 0; row < rows; ++row) {
        float* __restrict rp = tile.data + row * stride;
        for (uint32_t col = 0; col < cols; ++col) {
            float* __restrict px = rp + col * ch;

            // Exposure
            float r = px[0] * exp_mul;
            float g = px[1] * exp_mul;
            float b = px[2] * exp_mul;

            if (do_tone) {
                // Use luminance BEFORE clamping for better mask resolution
                float lum = 0.2126f * r + 0.7152f * g + 0.0722f * b;

                // Highlights: targets lum 0.5-1.5 range, negative = recover (darken)
                // Use smooth rolloff that works on HDR values > 1.0
                if (do_hl) {
                    float mask = (lum > 0.0f) ? (lum * lum) / (lum * lum + 0.25f) : 0.0f;
                    float adj = 1.0f - hl_str * mask * 0.7f;
                    r *= adj; g *= adj; b *= adj;
                }

                // Shadows: targets dark values, positive = lift
                if (do_sh) {
                    float inv_lum = 1.0f / (lum + 0.02f);
                    float mask = 1.0f / (1.0f + lum * 8.0f); // strong in darks
                    float adj = 1.0f + sh_str * mask * 1.5f;
                    r *= adj; g *= adj; b *= adj;
                }

                // Whites: extreme highlights (lum > 0.8), affects clipping
                if (do_wh) {
                    float mask = vega_clamp01((lum - 0.5f) * 2.0f);
                    mask = mask * mask * mask; // cubic, very top-heavy
                    float adj = 1.0f + wh_str * mask * 1.0f;
                    r *= adj; g *= adj; b *= adj;
                }

                // Blacks: extreme shadows (lum < 0.15), affects black point
                if (do_bk) {
                    float mask = vega_clamp01(1.0f - lum * 8.0f);
                    mask = mask * mask * mask;
                    float adj = 1.0f + bk_str * mask * 1.0f;
                    r *= adj; g *= adj; b *= adj;
                }
            }

            // Contrast
            if (do_contrast) {
                r = vega_clamp01(r);
                g = vega_clamp01(g);
                b = vega_clamp01(b);
                if (contrast_pos) {
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
