#define NOMINMAX
#include "pipeline/cpu/DenoiseNode.h"
#include "pipeline/cpu/FastMath.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace vega {

// Separable box blur on a single-channel buffer (in-place, using temp)
static void boxBlur(float* data, uint32_t w, uint32_t h, int radius)
{
    if (radius < 1) return;
    float inv = 1.0f / (2 * radius + 1);

    std::vector<float> tmp(w * h);

    // Horizontal
    for (uint32_t y = 0; y < h; ++y) {
        float sum = 0;
        for (int x = -radius; x <= radius; ++x)
            sum += data[y * w + std::clamp(x, 0, static_cast<int>(w) - 1)];
        tmp[y * w] = sum * inv;
        for (uint32_t x = 1; x < w; ++x) {
            int add = std::min(static_cast<int>(x) + radius, static_cast<int>(w) - 1);
            int rem = std::max(static_cast<int>(x) - radius - 1, 0);
            sum += data[y * w + add] - data[y * w + rem];
            tmp[y * w + x] = sum * inv;
        }
    }

    // Vertical
    for (uint32_t x = 0; x < w; ++x) {
        float sum = 0;
        for (int y = -radius; y <= radius; ++y)
            sum += tmp[std::clamp(y, 0, static_cast<int>(h) - 1) * w + x];
        data[x] = sum * inv;
        for (uint32_t y = 1; y < h; ++y) {
            int add = std::min(static_cast<int>(y) + radius, static_cast<int>(h) - 1);
            int rem = std::max(static_cast<int>(y) - radius - 1, 0);
            sum += tmp[add * w + x] - tmp[rem * w + x];
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

    const float luma_str = recipe.denoise_luminance / 100.0f;
    const float chroma_str = recipe.denoise_color / 100.0f;
    const float detail_keep = recipe.denoise_detail / 100.0f;

    VEGA_LOG_DEBUG("DenoiseNode: luma={:.0f} color={:.0f} detail={:.0f}",
        recipe.denoise_luminance, recipe.denoise_color, recipe.denoise_detail);

    const uint32_t npix = w * h;

    // Separate into Y, Cb, Cr (BT.601)
    std::vector<float> Y(npix), Cb(npix), Cr(npix);
    for (uint32_t y = 0; y < h; ++y) {
        const float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            const float* px = row + x * ch;
            float r = px[0], g = px[1], b = px[2];
            uint32_t idx = y * w + x;
            Y[idx]  =  0.299f * r + 0.587f * g + 0.114f * b;
            Cb[idx] = -0.169f * r - 0.331f * g + 0.500f * b;
            Cr[idx] =  0.500f * r - 0.419f * g - 0.081f * b;
        }
    }

    // Luminance denoise: edge-preserving via guided filter approach
    // Fast approximation: multi-pass box blur blended with original based on local variance
    if (luma_str > 0.01f) {
        int radius = 1 + static_cast<int>(luma_str * 3.0f);

        // Make a blurred copy
        std::vector<float> Y_blur = Y;
        // Multiple passes of box blur approximate Gaussian
        boxBlur(Y_blur.data(), w, h, radius);
        boxBlur(Y_blur.data(), w, h, radius);

        // Blend: mix = luma_str * (1 - detail_keep * local_edge)
        // Simple version: just lerp based on strength and detail preservation
        float base_mix = luma_str * (1.0f - detail_keep * 0.5f);
        base_mix = std::clamp(base_mix, 0.0f, 0.95f);

        for (uint32_t i = 0; i < npix; ++i) {
            float diff = std::abs(Y[i] - Y_blur[i]);
            // Preserve edges: reduce blur where difference is large
            float edge_factor = std::min(diff * 20.0f, 1.0f);
            float mix = base_mix * (1.0f - edge_factor * detail_keep);
            Y[i] = Y[i] * (1.0f - mix) + Y_blur[i] * mix;
        }
    }

    // Chroma denoise: stronger blur is acceptable (human eye less sensitive)
    if (chroma_str > 0.01f) {
        int radius = 2 + static_cast<int>(chroma_str * 4.0f);
        boxBlur(Cb.data(), w, h, radius);
        boxBlur(Cb.data(), w, h, radius);
        boxBlur(Cr.data(), w, h, radius);
        boxBlur(Cr.data(), w, h, radius);
    }

    // Convert back to RGB
    for (uint32_t y = 0; y < h; ++y) {
        float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            float* px = row + x * ch;
            uint32_t idx = y * w + x;
            float yy = Y[idx], cb = Cb[idx], cr = Cr[idx];
            px[0] = yy + 1.402f * cr;
            px[1] = yy - 0.344f * cb - 0.714f * cr;
            px[2] = yy + 1.772f * cb;
        }
    }
}

} // namespace vega
