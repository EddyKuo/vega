#define NOMINMAX
#include "pipeline/cpu/SharpenNode.h"
#include "pipeline/cpu/FastMath.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <cstring>

namespace vega {

// Same stackBlur as DenoiseNode — kept as static local to avoid header dependency.
static void stackBlur1D(float* __restrict data, float* __restrict tmp,
                         uint32_t w, uint32_t h, int radius)
{
    if (radius < 1) return;
    const float inv_sum = 1.0f / static_cast<float>((radius + 1) * (radius + 1));

    for (uint32_t y = 0; y < h; ++y) {
        float* __restrict src = data + y * w;
        float* __restrict dst = tmp + y * w;
        float stack_sum = 0, in_sum = 0, out_sum = 0;
        for (int i = -radius; i <= radius; ++i) {
            float val = src[std::clamp(i, 0, static_cast<int>(w) - 1)];
            stack_sum += val * (radius + 1 - std::abs(i));
            if (i <= 0) out_sum += val;
            if (i >= 0) in_sum += val;
        }
        for (uint32_t x = 0; x < w; ++x) {
            dst[x] = stack_sum * inv_sum;
            int left = static_cast<int>(x) - radius;
            stack_sum -= out_sum;
            out_sum -= src[std::clamp(left, 0, static_cast<int>(w) - 1)];
            int right = static_cast<int>(x) + radius + 1;
            in_sum += src[std::clamp(right, 0, static_cast<int>(w) - 1)];
            stack_sum += in_sum;
            int mid = std::clamp(static_cast<int>(x) + 1, 0, static_cast<int>(w) - 1);
            out_sum += src[mid];
            in_sum -= src[mid];
        }
    }
    for (uint32_t x = 0; x < w; ++x) {
        float stack_sum = 0, in_sum = 0, out_sum = 0;
        for (int i = -radius; i <= radius; ++i) {
            float val = tmp[std::clamp(i, 0, static_cast<int>(h) - 1) * w + x];
            stack_sum += val * (radius + 1 - std::abs(i));
            if (i <= 0) out_sum += val;
            if (i >= 0) in_sum += val;
        }
        for (uint32_t y = 0; y < h; ++y) {
            data[y * w + x] = stack_sum * inv_sum;
            int top = static_cast<int>(y) - radius;
            stack_sum -= out_sum;
            out_sum -= tmp[std::clamp(top, 0, static_cast<int>(h) - 1) * w + x];
            int bot = static_cast<int>(y) + radius + 1;
            in_sum += tmp[std::clamp(bot, 0, static_cast<int>(h) - 1) * w + x];
            stack_sum += in_sum;
            int mid = std::clamp(static_cast<int>(y) + 1, 0, static_cast<int>(h) - 1);
            out_sum += tmp[mid * w + x];
            in_sum -= tmp[mid * w + x];
        }
    }
}

void SharpenNode::process(Tile& tile, const EditRecipe& recipe)
{
    if (recipe.sharpen_amount < 0.5f)
        return;

    const uint32_t w = tile.width;
    const uint32_t h = tile.height;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;
    const uint32_t npix = w * h;
    const float amount = recipe.sharpen_amount / 50.0f;
    const int kr = std::max(1, static_cast<int>(recipe.sharpen_radius + 0.5f));
    const float masking = recipe.sharpen_masking / 100.0f;

    // Extract luminance + blur it
    std::vector<float> lum(npix), blurred(npix), tmp(npix);

    for (uint32_t y = 0; y < h; ++y) {
        const float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            const float* px = row + x * ch;
            lum[y * w + x] = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
        }
    }

    std::memcpy(blurred.data(), lum.data(), npix * sizeof(float));
    stackBlur1D(blurred.data(), tmp.data(), w, h, kr);

    // Apply USM with optional edge masking
    if (masking > 0.01f) {
        // Compute edge magnitude for masking
        float max_edge = 0;
        for (uint32_t y = 1; y + 1 < h; ++y) {
            for (uint32_t x = 1; x + 1 < w; ++x) {
                float gx = lum[y * w + x + 1] - lum[y * w + x - 1];
                float gy = lum[(y + 1) * w + x] - lum[(y - 1) * w + x];
                float mag = std::abs(gx) + std::abs(gy);
                tmp[y * w + x] = mag;  // reuse tmp for edge mask
                if (mag > max_edge) max_edge = mag;
            }
        }
        float thresh = masking * max_edge;
        float inv_t = 1.0f / (thresh + 1e-6f);

        for (uint32_t y = 0; y < h; ++y) {
            float* __restrict row = tile.data + y * stride;
            for (uint32_t x = 0; x < w; ++x) {
                float detail = lum[y * w + x] - blurred[y * w + x];
                float edge = (y > 0 && y + 1 < h && x > 0 && x + 1 < w)
                    ? std::min(tmp[y * w + x] * inv_t, 1.0f) : 1.0f;
                float adj = amount * edge * detail;
                float* px = row + x * ch;
                px[0] += adj; px[1] += adj; px[2] += adj;
            }
        }
    } else {
        for (uint32_t y = 0; y < h; ++y) {
            float* __restrict row = tile.data + y * stride;
            for (uint32_t x = 0; x < w; ++x) {
                float detail = lum[y * w + x] - blurred[y * w + x];
                float adj = amount * detail;
                float* px = row + x * ch;
                px[0] += adj; px[1] += adj; px[2] += adj;
            }
        }
    }
}

} // namespace vega
