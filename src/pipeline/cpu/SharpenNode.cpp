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
    const uint32_t npix = w * h;
    const float amount = recipe.sharpen_amount / 50.0f;
    const int kr = std::max(1, static_cast<int>(recipe.sharpen_radius + 0.5f));
    const float masking = recipe.sharpen_masking / 100.0f;
    const float inv_ksize = 1.0f / (2 * kr + 1);
    const int iw = static_cast<int>(w);
    const int ih = static_cast<int>(h);

    // Extract luminance
    std::vector<float> lum(npix);
    for (uint32_t y = 0; y < h; ++y) {
        const float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            const float* px = row + x * ch;
            lum[y * w + x] = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
        }
    }

    // Separable box blur with pre-allocated temp
    std::vector<float> blurred(npix);
    std::vector<float> tmp(npix);

    // Horizontal
    for (uint32_t y = 0; y < h; ++y) {
        const float* src = lum.data() + y * w;
        float* dst = tmp.data() + y * w;
        float sum = src[0] * (kr + 1);
        for (int x = 1; x <= kr; ++x)
            sum += src[std::min(x, iw - 1)];
        for (uint32_t x = 0; x < w; ++x) {
            int right = std::min(static_cast<int>(x) + kr + 1, iw - 1);
            int left = static_cast<int>(x) - kr;
            sum += src[right] - src[std::max(left, 0)];
            dst[x] = sum * inv_ksize;
        }
    }

    // Vertical
    for (uint32_t x = 0; x < w; ++x) {
        float sum = tmp[x] * (kr + 1);
        for (int y = 1; y <= kr; ++y)
            sum += tmp[std::min(y, ih - 1) * w + x];
        for (uint32_t y = 0; y < h; ++y) {
            int bot = std::min(static_cast<int>(y) + kr + 1, ih - 1);
            int top = static_cast<int>(y) - kr;
            sum += tmp[bot * w + x] - tmp[std::max(top, 0) * w + x];
            blurred[y * w + x] = sum * inv_ksize;
        }
    }

    // Edge mask (only if masking > 0)
    std::vector<float> edge;
    if (masking > 0.01f) {
        edge.resize(npix, 1.0f);
        float max_edge = 0;
        for (uint32_t y = 1; y + 1 < h; ++y) {
            for (uint32_t x = 1; x + 1 < w; ++x) {
                float gx = lum[y * w + x + 1] - lum[y * w + x - 1];
                float gy = lum[(y + 1) * w + x] - lum[(y - 1) * w + x];
                float mag = std::abs(gx) + std::abs(gy);
                edge[y * w + x] = mag;
                if (mag > max_edge) max_edge = mag;
            }
        }
        if (max_edge > 0) {
            float thresh = masking * max_edge;
            float inv_thresh = 1.0f / (thresh + 1e-6f);
            for (uint32_t i = 0; i < npix; ++i)
                edge[i] = (edge[i] > thresh) ? 1.0f : edge[i] * inv_thresh;
        }
    }

    // Apply USM
    for (uint32_t y = 0; y < h; ++y) {
        float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            float detail = lum[y * w + x] - blurred[y * w + x];
            float str = amount;
            if (!edge.empty()) str *= edge[y * w + x];
            float adj = str * detail;
            float* px = row + x * ch;
            px[0] += adj;
            px[1] += adj;
            px[2] += adj;
        }
    }
}

} // namespace vega
