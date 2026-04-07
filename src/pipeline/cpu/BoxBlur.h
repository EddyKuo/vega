#pragma once
#include <cstdint>
#include <algorithm>

namespace vega {

// Cache-friendly separable box blur.
// Vertical pass uses transpose to avoid column-major cache thrashing.
// tmp must be at least w*h floats. tmp2 must be at least w*h floats.
inline void boxBlurFast(float* __restrict data, float* __restrict tmp,
                        float* __restrict tmp2, uint32_t w, uint32_t h, int radius)
{
    if (radius < 1) return;
    const float inv = 1.0f / (2 * radius + 1);
    const int iw = static_cast<int>(w);
    const int ih = static_cast<int>(h);

    // Horizontal pass: data -> tmp (row-major, cache friendly)
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

    // Transpose tmp -> tmp2 (now columns become rows)
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            tmp2[x * h + y] = tmp[y * w + x];

    // "Vertical" pass as horizontal on transposed data: tmp2 -> tmp2 in-place via data
    for (uint32_t col = 0; col < w; ++col) {
        float* __restrict src = tmp2 + col * h;  // row in transposed = column in original
        float* __restrict dst = data + col * h;  // reuse data as temp (will transpose back)
        float sum = 0;
        for (int y = -radius; y <= radius; ++y)
            sum += src[std::clamp(y, 0, ih - 1)];
        dst[0] = sum * inv;
        for (uint32_t y = 1; y < h; ++y) {
            sum += src[std::min(static_cast<int>(y) + radius, ih - 1)]
                 - src[std::max(static_cast<int>(y) - radius - 1, 0)];
            dst[y] = sum * inv;
        }
    }

    // Transpose back: data (w rows of h) -> data (h rows of w)
    // data currently holds transposed result, copy to tmp then transpose back
    // Actually we stored in data as [col][row], need to put back as [row][col]
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            tmp[y * w + x] = data[x * h + y];

    // Copy final result back
    for (uint32_t i = 0; i < w * h; ++i)
        data[i] = tmp[i];
}

// Simpler version with just 2 buffers (data + tmp), slightly slower vertical pass
// but still correct and much simpler
inline void boxBlur2(float* __restrict data, float* __restrict tmp,
                     uint32_t w, uint32_t h, int radius)
{
    if (radius < 1) return;
    const float inv = 1.0f / (2 * radius + 1);
    const int iw = static_cast<int>(w);
    const int ih = static_cast<int>(h);

    // Horizontal: data -> tmp
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

    // Vertical: tmp -> data
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

} // namespace vega
