#define NOMINMAX
#include "pipeline/cpu/SharpenNode.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <vector>

namespace vega {

// Convert RGB to Lab (approximate, D65)
static void rgbToLab(float r, float g, float b, float& L, float& a, float& lab_b)
{
    // Linear sRGB to XYZ (D65)
    float X = 0.4124564f * r + 0.3575761f * g + 0.1804375f * b;
    float Y = 0.2126729f * r + 0.7151522f * g + 0.0721750f * b;
    float Z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * b;

    // Normalize by D65 white point
    X /= 0.95047f; Y /= 1.0f; Z /= 1.08883f;

    auto f = [](float t) -> float {
        return (t > 0.008856f) ? std::cbrt(t) : (7.787f * t + 16.0f / 116.0f);
    };

    L = 116.0f * f(Y) - 16.0f;
    a = 500.0f * (f(X) - f(Y));
    lab_b = 200.0f * (f(Y) - f(Z));
}

// Convert Lab to RGB (approximate, D65)
static void labToRgb(float L, float a, float lab_b, float& r, float& g, float& b)
{
    float fy = (L + 16.0f) / 116.0f;
    float fx = a / 500.0f + fy;
    float fz = fy - lab_b / 200.0f;

    auto inv_f = [](float t) -> float {
        return (t > 0.206893f) ? t * t * t : (t - 16.0f / 116.0f) / 7.787f;
    };

    float X = 0.95047f * inv_f(fx);
    float Y = 1.0f     * inv_f(fy);
    float Z = 1.08883f * inv_f(fz);

    // XYZ to linear sRGB
    r =  3.2404542f * X - 1.5371385f * Y - 0.4985314f * Z;
    g = -0.9692660f * X + 1.8760108f * Y + 0.0415560f * Z;
    b =  0.0556434f * X - 0.2040259f * Y + 1.0572252f * Z;
}

void SharpenNode::process(Tile& tile, const EditRecipe& recipe)
{
    if (recipe.sharpen_amount < 0.01f)
        return;

    const uint32_t w = tile.width;
    const uint32_t h = tile.height;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;
    const float amount = recipe.sharpen_amount / 100.0f;  // 0-1.5
    const float radius = recipe.sharpen_radius;             // 0.5-3.0
    const float masking = recipe.sharpen_masking / 100.0f;  // 0-1

    VEGA_LOG_DEBUG("SharpenNode: amount={:.1f} radius={:.1f} masking={:.0f}",
        recipe.sharpen_amount, radius, recipe.sharpen_masking);

    // Extract luminance channel (L from Lab)
    std::vector<float> lum(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        const float* row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            const float* px = row + x * ch;
            float L, a, b;
            rgbToLab(std::max(0.f, px[0]), std::max(0.f, px[1]), std::max(0.f, px[2]), L, a, b);
            lum[y * w + x] = L;
        }
    }

    // Gaussian blur of luminance (separable, box approximation for speed)
    // Use integer radius from the float parameter
    int kr = std::max(1, static_cast<int>(radius + 0.5f));
    int ksize = kr * 2 + 1;

    // Horizontal pass
    std::vector<float> blur_h(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float sum = 0;
            int count = 0;
            for (int dx = -kr; dx <= kr; ++dx) {
                int sx = std::clamp(static_cast<int>(x) + dx, 0, static_cast<int>(w) - 1);
                sum += lum[y * w + sx];
                count++;
            }
            blur_h[y * w + x] = sum / count;
        }
    }

    // Vertical pass
    std::vector<float> blurred(w * h);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            float sum = 0;
            int count = 0;
            for (int dy = -kr; dy <= kr; ++dy) {
                int sy = std::clamp(static_cast<int>(y) + dy, 0, static_cast<int>(h) - 1);
                sum += blur_h[sy * w + x];
                count++;
            }
            blurred[y * w + x] = sum / count;
        }
    }

    // Edge mask (Sobel gradient magnitude) for masking parameter
    std::vector<float> edge_mask;
    if (masking > 0.01f) {
        edge_mask.resize(w * h);
        float max_edge = 0;
        for (uint32_t y = 1; y + 1 < h; ++y) {
            for (uint32_t x = 1; x + 1 < w; ++x) {
                float gx = -lum[(y-1)*w+(x-1)] + lum[(y-1)*w+(x+1)]
                           -2*lum[y*w+(x-1)]   + 2*lum[y*w+(x+1)]
                           -lum[(y+1)*w+(x-1)] + lum[(y+1)*w+(x+1)];
                float gy = -lum[(y-1)*w+(x-1)] - 2*lum[(y-1)*w+x] - lum[(y-1)*w+(x+1)]
                           +lum[(y+1)*w+(x-1)] + 2*lum[(y+1)*w+x] + lum[(y+1)*w+(x+1)];
                float mag = std::sqrt(gx * gx + gy * gy);
                edge_mask[y * w + x] = mag;
                if (mag > max_edge) max_edge = mag;
            }
        }
        // Normalize and apply masking threshold
        if (max_edge > 0) {
            float inv = 1.0f / max_edge;
            for (auto& v : edge_mask) {
                v = std::clamp(v * inv - masking, 0.0f, 1.0f) / (1.0f - masking + 0.001f);
            }
        }
    }

    // Apply Unsharp Mask: sharpened = original + amount * (original - blurred)
    for (uint32_t y = 0; y < h; ++y) {
        float* row = tile.data + y * stride;
        for (uint32_t x = 0; x < w; ++x) {
            float* px = row + x * ch;
            float L_orig = lum[y * w + x];
            float L_blur = blurred[y * w + x];
            float detail = L_orig - L_blur;

            float strength = amount;
            if (!edge_mask.empty())
                strength *= edge_mask[y * w + x];

            float L_sharp = L_orig + strength * detail;

            // Apply: scale RGB by ratio of sharpened L to original L
            if (L_orig > 0.1f) {
                float ratio = L_sharp / L_orig;
                ratio = std::clamp(ratio, 0.5f, 2.0f);
                px[0] *= ratio;
                px[1] *= ratio;
                px[2] *= ratio;
            }
        }
    }
}

} // namespace vega
