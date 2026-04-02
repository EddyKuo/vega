#define NOMINMAX
#include "pipeline/cpu/DenoiseNode.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace vega {

// RGB to YCbCr (BT.601)
static void rgbToYCbCr(float r, float g, float b, float& Y, float& Cb, float& Cr)
{
    Y  =  0.299f * r + 0.587f * g + 0.114f * b;
    Cb = -0.169f * r - 0.331f * g + 0.500f * b;
    Cr =  0.500f * r - 0.419f * g - 0.081f * b;
}

// YCbCr to RGB (BT.601)
static void ycbcrToRgb(float Y, float Cb, float Cr, float& r, float& g, float& b)
{
    r = Y + 1.402f * Cr;
    g = Y - 0.344f * Cb - 0.714f * Cr;
    b = Y + 1.772f * Cb;
}

void DenoiseNode::process(Tile& tile, const EditRecipe& recipe)
{
    if (recipe.denoise_luminance < 0.01f && recipe.denoise_color < 0.01f)
        return;

    const uint32_t w = tile.width;
    const uint32_t h = tile.height;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    const float luma_strength = recipe.denoise_luminance / 100.0f;   // 0-1
    const float chroma_strength = recipe.denoise_color / 100.0f;     // 0-1
    const float detail_preserve = recipe.denoise_detail / 100.0f;    // 0-1

    VEGA_LOG_DEBUG("DenoiseNode: luma={:.0f} color={:.0f} detail={:.0f}",
        recipe.denoise_luminance, recipe.denoise_color, recipe.denoise_detail);

    // Convert to YCbCr
    std::vector<float> Y_ch(w * h), Cb_ch(w * h), Cr_ch(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        const float* row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            const float* px = row + x * ch;
            rgbToYCbCr(px[0], px[1], px[2],
                       Y_ch[y * w + x], Cb_ch[y * w + x], Cr_ch[y * w + x]);
        }
    }

    // Bilateral filter parameters
    // Spatial sigma scales with strength, range sigma controls edge preservation
    int radius = std::max(1, static_cast<int>(2.0f + luma_strength * 3.0f));
    float sigma_s = 1.0f + luma_strength * 4.0f;
    float sigma_r_luma = 0.01f + luma_strength * 0.08f * (1.0f - detail_preserve * 0.5f);
    float sigma_r_chroma = 0.01f + chroma_strength * 0.12f;

    float inv_2s2 = -0.5f / (sigma_s * sigma_s);
    float inv_2r2_luma = -0.5f / (sigma_r_luma * sigma_r_luma);
    float inv_2r2_chroma = -0.5f / (sigma_r_chroma * sigma_r_chroma);

    // Output buffers
    std::vector<float> Y_out(w * h), Cb_out(w * h), Cr_out(w * h);

    // Bilateral filter on Y (luminance)
    if (luma_strength > 0.01f) {
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                float center = Y_ch[y * w + x];
                float sum_w = 0, sum_v = 0;

                for (int dy = -radius; dy <= radius; ++dy) {
                    int sy = std::clamp(static_cast<int>(y) + dy, 0, static_cast<int>(h) - 1);
                    for (int dx = -radius; dx <= radius; ++dx) {
                        int sx = std::clamp(static_cast<int>(x) + dx, 0, static_cast<int>(w) - 1);

                        float val = Y_ch[sy * w + sx];
                        float dist2 = static_cast<float>(dx * dx + dy * dy);
                        float diff = val - center;

                        float weight = std::exp(dist2 * inv_2s2 + diff * diff * inv_2r2_luma);
                        sum_w += weight;
                        sum_v += weight * val;
                    }
                }
                Y_out[y * w + x] = sum_v / (sum_w + 1e-10f);
            }
        }
    } else {
        Y_out = Y_ch;
    }

    // Bilateral filter on Cb, Cr (chroma) with larger kernel
    if (chroma_strength > 0.01f) {
        int cr_radius = radius + 1;
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                float center_cb = Cb_ch[y * w + x];
                float center_cr = Cr_ch[y * w + x];
                float sum_w = 0, sum_cb = 0, sum_cr = 0;

                for (int dy = -cr_radius; dy <= cr_radius; ++dy) {
                    int sy = std::clamp(static_cast<int>(y) + dy, 0, static_cast<int>(h) - 1);
                    for (int dx = -cr_radius; dx <= cr_radius; ++dx) {
                        int sx = std::clamp(static_cast<int>(x) + dx, 0, static_cast<int>(w) - 1);

                        float cb = Cb_ch[sy * w + sx];
                        float cr = Cr_ch[sy * w + sx];
                        float dist2 = static_cast<float>(dx * dx + dy * dy);
                        float diff_cb = cb - center_cb;
                        float diff_cr = cr - center_cr;
                        float range2 = diff_cb * diff_cb + diff_cr * diff_cr;

                        float weight = std::exp(dist2 * inv_2s2 + range2 * inv_2r2_chroma);
                        sum_w += weight;
                        sum_cb += weight * cb;
                        sum_cr += weight * cr;
                    }
                }
                float inv_w = 1.0f / (sum_w + 1e-10f);
                Cb_out[y * w + x] = sum_cb * inv_w;
                Cr_out[y * w + x] = sum_cr * inv_w;
            }
        }
    } else {
        Cb_out = Cb_ch;
        Cr_out = Cr_ch;
    }

    // Convert back to RGB
    for (uint32_t y = 0; y < h; ++y) {
        float* row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            float* px = row + x * ch;
            ycbcrToRgb(Y_out[y * w + x], Cb_out[y * w + x], Cr_out[y * w + x],
                       px[0], px[1], px[2]);
        }
    }
}

} // namespace vega
