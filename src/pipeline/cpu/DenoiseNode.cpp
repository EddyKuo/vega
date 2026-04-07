#define NOMINMAX
#include "pipeline/cpu/DenoiseNode.h"
#include "pipeline/cpu/FastMath.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace vega {

// In-place separable box blur using pre-allocated temp buffer
static void boxBlurFast(float* data, float* tmp, uint32_t w, uint32_t h, int radius)
{
    if (radius < 1) return;
    const float inv = 1.0f / (2 * radius + 1);
    const int iw = static_cast<int>(w);
    const int ih = static_cast<int>(h);

    // Horizontal
    for (uint32_t y = 0; y < h; ++y) {
        float* src = data + y * w;
        float* dst = tmp + y * w;
        float sum = src[0] * (radius + 1);
        for (int x = 1; x <= radius; ++x)
            sum += src[std::min(x, iw - 1)];
        for (uint32_t x = 0; x < w; ++x) {
            int right = std::min(static_cast<int>(x) + radius + 1, iw - 1);
            int left = static_cast<int>(x) - radius;
            if (left >= 0)
                sum += src[right] - src[left];
            else
                sum += src[right] - src[0];
            dst[x] = sum * inv;
        }
    }

    // Vertical
    for (uint32_t x = 0; x < w; ++x) {
        float sum = tmp[x] * (radius + 1);
        for (int y = 1; y <= radius; ++y)
            sum += tmp[std::min(y, ih - 1) * w + x];
        for (uint32_t y = 0; y < h; ++y) {
            int bot = std::min(static_cast<int>(y) + radius + 1, ih - 1);
            int top = static_cast<int>(y) - radius;
            if (top >= 0)
                sum += tmp[bot * w + x] - tmp[top * w + x];
            else
                sum += tmp[bot * w + x] - tmp[x];
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

    // Single temp buffer shared by all blur passes
    std::vector<float> tmp(npix);

    // Separate into Y, Cb, Cr in-place arrays
    std::vector<float> Y(npix), Cb(npix), Cr(npix);
    for (uint32_t y = 0; y < h; ++y) {
        const float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            const float* px = row + x * ch;
            uint32_t idx = y * w + x;
            Y[idx]  =  0.299f * px[0] + 0.587f * px[1] + 0.114f * px[2];
            Cb[idx] = -0.169f * px[0] - 0.331f * px[1] + 0.500f * px[2];
            Cr[idx] =  0.500f * px[0] - 0.419f * px[1] - 0.081f * px[2];
        }
    }

    // Luminance denoise: single-pass box blur + edge-preserving blend
    if (luma_str > 0.01f) {
        int radius = std::max(1, static_cast<int>(luma_str * 2.0f + 0.5f));
        std::vector<float> Y_blur(Y);
        boxBlurFast(Y_blur.data(), tmp.data(), w, h, radius);

        float base_mix = luma_str * (1.0f - detail_keep * 0.5f);
        base_mix = std::clamp(base_mix, 0.0f, 0.95f);

        for (uint32_t i = 0; i < npix; ++i) {
            float diff = std::abs(Y[i] - Y_blur[i]);
            float edge = std::min(diff * 20.0f, 1.0f);
            float mix = base_mix * (1.0f - edge * detail_keep);
            Y[i] += (Y_blur[i] - Y[i]) * mix;
        }
    }

    // Chroma denoise: single-pass box blur (eye less sensitive)
    if (chroma_str > 0.01f) {
        int radius = std::max(1, static_cast<int>(chroma_str * 3.0f + 0.5f));
        boxBlurFast(Cb.data(), tmp.data(), w, h, radius);
        boxBlurFast(Cr.data(), tmp.data(), w, h, radius);
    }

    // Convert back to RGB
    for (uint32_t y = 0; y < h; ++y) {
        float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            float* px = row + x * ch;
            uint32_t idx = y * w + x;
            px[0] = Y[idx] + 1.402f * Cr[idx];
            px[1] = Y[idx] - 0.344f * Cb[idx] - 0.714f * Cr[idx];
            px[2] = Y[idx] + 1.772f * Cb[idx];
        }
    }
}

} // namespace vega
