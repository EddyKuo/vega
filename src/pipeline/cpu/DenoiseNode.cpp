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

// Correct separable box blur: horizontal then vertical, using shared tmp.
static void boxBlur(float* __restrict data, float* __restrict tmp,
                    uint32_t w, uint32_t h, int radius)
{
    if (radius < 1) return;
    const int ksize = 2 * radius + 1;
    const float inv = 1.0f / ksize;
    const int iw = static_cast<int>(w);
    const int ih = static_cast<int>(h);

    // Horizontal: data -> tmp (running sum)
    for (uint32_t y = 0; y < h; ++y) {
        const float* __restrict src = data + y * w;
        float* __restrict dst = tmp + y * w;
        float sum = 0;
        for (int x = -radius; x <= radius; ++x)
            sum += src[std::clamp(x, 0, iw - 1)];
        dst[0] = sum * inv;
        for (uint32_t x = 1; x < w; ++x) {
            sum += src[std::min(static_cast<int>(x) + radius, iw - 1)]
                 - src[std::max(static_cast<int>(x) - radius - 1, 0)];
            dst[x] = sum * inv;
        }
    }

    // Vertical: tmp -> data (running sum)
    for (uint32_t x = 0; x < w; ++x) {
        float sum = 0;
        for (int y = -radius; y <= radius; ++y)
            sum += tmp[std::clamp(y, 0, ih - 1) * w + x];
        data[x] = sum * inv;
        for (uint32_t y = 1; y < h; ++y) {
            sum += tmp[std::min(static_cast<int>(y) + radius, ih - 1) * w + x]
                 - tmp[std::max(static_cast<int>(y) - radius - 1, 0) * w + x];
            data[y * w + x] = sum * inv;
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

    // Shared temp buffers — allocated once
    std::vector<float> tmp(npix);
    std::vector<float> Y(npix), Cb(npix), Cr(npix);

    // RGB -> YCbCr
    for (uint32_t y = 0; y < h; ++y) {
        const float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            const float* px = row + x * ch;
            uint32_t i = y * w + x;
            Y[i]  =  0.299f * px[0] + 0.587f * px[1] + 0.114f * px[2];
            Cb[i] = -0.169f * px[0] - 0.331f * px[1] + 0.500f * px[2];
            Cr[i] =  0.500f * px[0] - 0.419f * px[1] - 0.081f * px[2];
        }
    }

    // Luma denoise: blur Y, edge-preserving blend
    if (luma_str > 0.01f) {
        int radius = std::max(1, static_cast<int>(luma_str * 2.0f + 0.5f));
        std::vector<float> Y_blur(npix);
        std::memcpy(Y_blur.data(), Y.data(), npix * sizeof(float));
        boxBlur(Y_blur.data(), tmp.data(), w, h, radius);

        float base_mix = std::clamp(luma_str * (1.0f - detail_keep * 0.5f), 0.0f, 0.9f);
        for (uint32_t i = 0; i < npix; ++i) {
            float diff = std::abs(Y[i] - Y_blur[i]);
            float edge = std::min(diff * 15.0f, 1.0f);
            float mix = base_mix * (1.0f - edge * detail_keep);
            Y[i] += (Y_blur[i] - Y[i]) * mix;
        }
    }

    // Chroma denoise: blur Cb and Cr
    if (chroma_str > 0.01f) {
        int radius = std::max(1, static_cast<int>(chroma_str * 2.0f + 0.5f));
        boxBlur(Cb.data(), tmp.data(), w, h, radius);
        boxBlur(Cr.data(), tmp.data(), w, h, radius);
    }

    // YCbCr -> RGB
    for (uint32_t y = 0; y < h; ++y) {
        float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            float* px = row + x * ch;
            uint32_t i = y * w + x;
            px[0] = Y[i] + 1.402f * Cr[i];
            px[1] = Y[i] - 0.344f * Cb[i] - 0.714f * Cr[i];
            px[2] = Y[i] + 1.772f * Cb[i];
        }
    }
}

} // namespace vega
