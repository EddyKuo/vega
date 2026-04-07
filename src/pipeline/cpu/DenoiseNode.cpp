#define NOMINMAX
#include "pipeline/cpu/DenoiseNode.h"
#include "pipeline/cpu/FastMath.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>

namespace vega {

// StackBlur-style single-channel blur: O(1) per pixel, triangle-weighted.
// Operates in-place using pre-allocated tmp buffer.
// Much better visual quality than box blur at same speed.
static void stackBlur1D(float* __restrict data, float* __restrict tmp,
                         uint32_t w, uint32_t h, int radius)
{
    if (radius < 1) return;
    const int div = 2 * radius + 1;
    const float inv_sum = 1.0f / static_cast<float>((radius + 1) * (radius + 1));

    // Horizontal pass: data -> tmp
    for (uint32_t y = 0; y < h; ++y) {
        float* __restrict src = data + y * w;
        float* __restrict dst = tmp + y * w;

        float stack_sum = 0, in_sum = 0, out_sum = 0;

        // Pre-fill the stack
        float first = src[0];
        float last = src[w - 1];
        for (int i = -radius; i <= radius; ++i) {
            float val = src[std::clamp(i, 0, static_cast<int>(w) - 1)];
            int weight = radius + 1 - std::abs(i);
            stack_sum += val * weight;
            if (i <= 0) out_sum += val;
            if (i >= 0) in_sum += val;
        }

        for (uint32_t x = 0; x < w; ++x) {
            dst[x] = stack_sum * inv_sum;

            // Remove outgoing pixel
            int left = static_cast<int>(x) - radius;
            float out_val = src[std::clamp(left, 0, static_cast<int>(w) - 1)];
            stack_sum -= out_sum;
            out_sum -= out_val;

            // Add incoming pixel
            int right = static_cast<int>(x) + radius + 1;
            float in_val = src[std::clamp(right, 0, static_cast<int>(w) - 1)];
            in_sum += in_val;
            stack_sum += in_sum;

            out_sum += src[std::clamp(static_cast<int>(x) + 1, 0, static_cast<int>(w) - 1)];
            in_sum -= src[std::clamp(static_cast<int>(x) + 1, 0, static_cast<int>(w) - 1)];
        }
    }

    // Vertical pass: tmp -> data
    for (uint32_t x = 0; x < w; ++x) {
        float stack_sum = 0, in_sum = 0, out_sum = 0;

        float first = tmp[x];
        for (int i = -radius; i <= radius; ++i) {
            int sy = std::clamp(i, 0, static_cast<int>(h) - 1);
            float val = tmp[sy * w + x];
            int weight = radius + 1 - std::abs(i);
            stack_sum += val * weight;
            if (i <= 0) out_sum += val;
            if (i >= 0) in_sum += val;
        }

        for (uint32_t y = 0; y < h; ++y) {
            data[y * w + x] = stack_sum * inv_sum;

            int top = static_cast<int>(y) - radius;
            float out_val = tmp[std::clamp(top, 0, static_cast<int>(h) - 1) * w + x];
            stack_sum -= out_sum;
            out_sum -= out_val;

            int bot = static_cast<int>(y) + radius + 1;
            float in_val = tmp[std::clamp(bot, 0, static_cast<int>(h) - 1) * w + x];
            in_sum += in_val;
            stack_sum += in_sum;

            int mid = std::clamp(static_cast<int>(y) + 1, 0, static_cast<int>(h) - 1);
            out_sum += tmp[mid * w + x];
            in_sum -= tmp[mid * w + x];
        }
    }
}

void DenoiseNode::process(Tile& tile, const EditRecipe& recipe)
{
    if (recipe.denoise_luminance < 0.5f && recipe.denoise_color < 0.5f)
        return;

    const uint32_t w = tile.width;
    const uint32_t h = tile.height;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;
    const uint32_t npix = w * h;

    const float luma_str = recipe.denoise_luminance / 100.0f;
    const float chroma_str = recipe.denoise_color / 100.0f;
    const float detail_keep = recipe.denoise_detail / 100.0f;

    // Work directly on RGB channels — skip YCbCr conversion.
    // For color denoise, blur all 3 channels and blend.
    // For luma denoise, compute luminance, blur it, and scale RGB.
    std::vector<float> tmp(npix);  // shared temp for blur

    if (chroma_str > 0.01f) {
        // Color denoise: blur each RGB channel separately, blend with original
        int radius = std::max(1, static_cast<int>(chroma_str * 2.0f + 0.5f));
        float mix = std::clamp(chroma_str * 0.8f, 0.0f, 0.9f);

        // Process each channel in-place on tile data
        // Extract -> blur -> blend back
        std::vector<float> chan(npix);

        for (int c = 0; c < 3; ++c) {
            // Extract channel
            for (uint32_t y = 0; y < h; ++y) {
                const float* row = tile.data + y * stride;
                for (uint32_t x = 0; x < w; ++x)
                    chan[y * w + x] = row[x * ch + c];
            }

            // Blur
            stackBlur1D(chan.data(), tmp.data(), w, h, radius);

            // Blend back
            for (uint32_t y = 0; y < h; ++y) {
                float* row = tile.data + y * stride;
                for (uint32_t x = 0; x < w; ++x) {
                    float orig = row[x * ch + c];
                    row[x * ch + c] = orig + (chan[y * w + x] - orig) * mix;
                }
            }
        }
    }

    if (luma_str > 0.01f) {
        // Luma denoise: compute luminance, blur it, scale RGB to match
        int radius = std::max(1, static_cast<int>(luma_str * 1.5f + 0.5f));
        float mix = std::clamp(luma_str * (1.0f - detail_keep * 0.5f), 0.0f, 0.9f);

        std::vector<float> lum(npix), lum_blur(npix);
        for (uint32_t y = 0; y < h; ++y) {
            const float* row = tile.data + y * stride;
            for (uint32_t x = 0; x < w; ++x) {
                const float* px = row + x * ch;
                lum[y * w + x] = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
            }
        }

        std::memcpy(lum_blur.data(), lum.data(), npix * sizeof(float));
        stackBlur1D(lum_blur.data(), tmp.data(), w, h, radius);

        // Apply: scale RGB by blurred/original luminance ratio
        for (uint32_t y = 0; y < h; ++y) {
            float* row = tile.data + y * stride;
            for (uint32_t x = 0; x < w; ++x) {
                float* px = row + x * ch;
                float orig_l = lum[y * w + x];
                float blur_l = lum_blur[y * w + x];

                // Edge-preserving: reduce effect where difference is large
                float diff = std::abs(orig_l - blur_l);
                float edge = std::min(diff * 15.0f, 1.0f);
                float m = mix * (1.0f - edge * detail_keep);

                float target_l = orig_l + (blur_l - orig_l) * m;
                if (orig_l > 0.001f) {
                    float ratio = target_l / orig_l;
                    px[0] *= ratio;
                    px[1] *= ratio;
                    px[2] *= ratio;
                }
            }
        }
    }
}

} // namespace vega
