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

static void boxBlur(float* __restrict data, float* __restrict tmp,
                    uint32_t w, uint32_t h, int radius)
{
    if (radius < 1) return;
    const float inv = 1.0f / (2 * radius + 1);
    const int iw = static_cast<int>(w);
    const int ih = static_cast<int>(h);

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

    // Extract luminance
    std::vector<float> lum(npix), blurred(npix), tmp(npix);
    for (uint32_t y = 0; y < h; ++y) {
        const float* __restrict row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            const float* px = row + x * ch;
            lum[y * w + x] = 0.2126f * px[0] + 0.7152f * px[1] + 0.0722f * px[2];
        }
    }

    // Blur luminance
    std::memcpy(blurred.data(), lum.data(), npix * sizeof(float));
    boxBlur(blurred.data(), tmp.data(), w, h, kr);

    // USM with optional edge mask
    if (masking > 0.01f) {
        // Reuse tmp for edge magnitude
        float max_e = 0;
        for (uint32_t y = 1; y + 1 < h; ++y)
            for (uint32_t x = 1; x + 1 < w; ++x) {
                float gx = lum[y*w+x+1] - lum[y*w+x-1];
                float gy = lum[(y+1)*w+x] - lum[(y-1)*w+x];
                float m = std::abs(gx) + std::abs(gy);
                tmp[y*w+x] = m;
                if (m > max_e) max_e = m;
            }
        float thresh = masking * max_e;
        float inv_t = 1.0f / (thresh + 1e-6f);

        for (uint32_t y = 0; y < h; ++y) {
            float* __restrict row = tile.data + y * stride;
            for (uint32_t x = 0; x < w; ++x) {
                float d = lum[y*w+x] - blurred[y*w+x];
                float e = (y>0 && y+1<h && x>0 && x+1<w)
                    ? std::min(tmp[y*w+x] * inv_t, 1.0f) : 1.0f;
                float adj = amount * e * d;
                float* px = row + x * ch;
                px[0] += adj; px[1] += adj; px[2] += adj;
            }
        }
    } else {
        for (uint32_t y = 0; y < h; ++y) {
            float* __restrict row = tile.data + y * stride;
            for (uint32_t x = 0; x < w; ++x) {
                float d = lum[y*w+x] - blurred[y*w+x];
                float adj = amount * d;
                float* px = row + x * ch;
                px[0] += adj; px[1] += adj; px[2] += adj;
            }
        }
    }
}

} // namespace vega
