#include "pipeline/cpu/ColorSpaceNode.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <array>

namespace vega {

// Pre-computed sRGB gamma LUT (65536 entries for 16-bit precision)
static constexpr int LUT_SIZE = 65536;
static std::array<float, LUT_SIZE> s_gamma_lut;
static bool s_lut_initialized = false;

static void initGammaLUT()
{
    if (s_lut_initialized) return;
    for (int i = 0; i < LUT_SIZE; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(LUT_SIZE - 1);
        if (x < 0.0031308f)
            s_gamma_lut[i] = 12.92f * x;
        else
            s_gamma_lut[i] = 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
    }
    s_lut_initialized = true;
}

static inline float lookupGamma(float x)
{
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    int idx = static_cast<int>(x * (LUT_SIZE - 1) + 0.5f);
    return s_gamma_lut[idx];
}

void ColorSpaceNode::process(Tile& tile, const EditRecipe& recipe)
{
    initGammaLUT();

    const uint32_t rows = tile.height;
    const uint32_t cols = tile.width;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    for (uint32_t row = 0; row < rows; ++row) {
        float* rowPtr = tile.data + row * stride;
        for (uint32_t col = 0; col < cols; ++col) {
            float* px = rowPtr + col * ch;
            px[0] = lookupGamma(px[0]);
            px[1] = lookupGamma(px[1]);
            px[2] = lookupGamma(px[2]);
        }
    }
}

} // namespace vega
