#include "pipeline/cpu/ColorSpaceNode.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>

namespace vega {

float ColorSpaceNode::linearToSRGB(float x)
{
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;
    if (x < 0.0031308f)
        return 12.92f * x;
    return 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

void ColorSpaceNode::process(Tile& tile, const EditRecipe& recipe)
{
    // For now, only sRGB output is supported.
    // In the future, this node can branch based on recipe.output_colorspace.
    VEGA_LOG_DEBUG("ColorSpaceNode: applying sRGB gamma transfer");

    const uint32_t rows = tile.height;
    const uint32_t cols = tile.width;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    for (uint32_t row = 0; row < rows; ++row) {
        float* rowPtr = tile.data + row * stride;
        for (uint32_t col = 0; col < cols; ++col) {
            float* px = rowPtr + col * ch;
            px[0] = linearToSRGB(std::clamp(px[0], 0.0f, 1.0f));
            px[1] = linearToSRGB(std::clamp(px[1], 0.0f, 1.0f));
            px[2] = linearToSRGB(std::clamp(px[2], 0.0f, 1.0f));
        }
    }
}

} // namespace vega
