#define NOMINMAX
#include "pipeline/cpu/SharpenNode.h"
#include "pipeline/cpu/FastMath.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace vega {

void SharpenNode::process(Tile& tile, const EditRecipe& recipe)
{
    if (recipe.sharpen_amount < 0.5f)
        return;

    const uint32_t w = tile.width;
    const uint32_t h = tile.height;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;
    const float amount = recipe.sharpen_amount / 50.0f;   // 0-3.0 range for visible effect
    const float radius = recipe.sharpen_radius;
    const float masking = recipe.sharpen_masking / 100.0f;

    VEGA_LOG_DEBUG("SharpenNode: amount={:.1f} radius={:.1f} masking={:.0f}",
        recipe.sharpen_amount, radius, recipe.sharpen_masking);

    // Extract luminance (simple weighted sum, fast)
    std::vector<float> lum(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        const float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            const float* px = row + x * ch;
            lum[y * w + x] = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
        }
    }

    // Separable box blur (fast approximation of Gaussian)
    int kr = std::max(1, static_cast<int>(radius + 0.5f));
    float inv_ksize = 1.0f / (2 * kr + 1);

    // Horizontal pass
    std::vector<float> tmp(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        // Running sum for O(1) per pixel box blur
        float sum = 0;
        for (int x = -kr; x <= kr; ++x)
            sum += lum[y * w + std::clamp(x, 0, static_cast<int>(w) - 1)];
        tmp[y * w + 0] = sum * inv_ksize;

        for (uint32_t x = 1; x < w; ++x) {
            int add = std::min(static_cast<int>(x) + kr, static_cast<int>(w) - 1);
            int rem = std::max(static_cast<int>(x) - kr - 1, 0);
            sum += lum[y * w + add] - lum[y * w + rem];
            tmp[y * w + x] = sum * inv_ksize;
        }
    }

    // Vertical pass
    std::vector<float> blurred(w * h);
    for (uint32_t x = 0; x < w; ++x) {
        float sum = 0;
        for (int y = -kr; y <= kr; ++y)
            sum += tmp[std::clamp(y, 0, static_cast<int>(h) - 1) * w + x];
        blurred[x] = sum * inv_ksize;

        for (uint32_t y = 1; y < h; ++y) {
            int add = std::min(static_cast<int>(y) + kr, static_cast<int>(h) - 1);
            int rem = std::max(static_cast<int>(y) - kr - 1, 0);
            sum += tmp[add * w + x] - tmp[rem * w + x];
            blurred[y * w + x] = sum * inv_ksize;
        }
    }

    // Edge mask (only if masking > 0) using simple gradient magnitude
    std::vector<float> edge_mask;
    if (masking > 0.01f) {
        edge_mask.resize(w * h, 1.0f);
        float max_edge = 0;
        for (uint32_t y = 1; y + 1 < h; ++y) {
            for (uint32_t x = 1; x + 1 < w; ++x) {
                float gx = lum[y * w + (x+1)] - lum[y * w + (x-1)];
                float gy = lum[(y+1) * w + x] - lum[(y-1) * w + x];
                float mag = std::abs(gx) + std::abs(gy); // L1 norm, faster
                edge_mask[y * w + x] = mag;
                if (mag > max_edge) max_edge = mag;
            }
        }
        if (max_edge > 0) {
            float thresh = masking * max_edge;
            for (auto& v : edge_mask) {
                v = (v > thresh) ? 1.0f : v / (thresh + 1e-6f);
            }
        }
    }

    // Apply USM: pixel += amount * (original - blurred) * mask
    for (uint32_t y = 0; y < h; ++y) {
        float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            float* __restrict px = row + x * ch;
            float detail = lum[y * w + x] - blurred[y * w + x];

            float strength = amount;
            if (!edge_mask.empty())
                strength *= edge_mask[y * w + x];

            // Apply detail to each channel directly (preserves color)
            float adj = strength * detail;
            px[0] += adj;
            px[1] += adj;
            px[2] += adj;
        }
    }
}

} // namespace vega
