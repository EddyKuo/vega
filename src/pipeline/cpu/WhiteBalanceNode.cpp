#include "pipeline/cpu/WhiteBalanceNode.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>

namespace vega {

void WhiteBalanceNode::temperatureTintToRGB(float temperature_k, float tint,
                                            float& r_mul, float& g_mul, float& b_mul)
{
    // Planckian locus approximation for CIE xy chromaticity from CCT.
    // Based on Kim et al. and Hernandez-Andres et al. approximations.
    // Valid range roughly 1667K - 25000K.
    temperature_k = std::clamp(temperature_k, 1667.0f, 25000.0f);

    double T = static_cast<double>(temperature_k);
    double T2 = T * T;
    double T3 = T2 * T;

    // CIE x chromaticity coordinate from CCT
    double x;
    if (T <= 4000.0)
        x = -0.2661239e9 / T3 - 0.2343589e6 / T2 + 0.8776956e3 / T + 0.179910;
    else
        x = -3.0258469e9 / T3 + 2.1070379e6 / T2 + 0.2226347e3 / T + 0.240390;

    // CIE y chromaticity coordinate from x
    double x2 = x * x;
    double x3 = x2 * x;
    double y;
    if (T <= 2222.0)
        y = -1.1063814 * x3 - 1.34811020 * x2 + 2.18555832 * x - 0.20219683;
    else if (T <= 4000.0)
        y = -0.9549476 * x3 - 1.37418593 * x2 + 2.09137015 * x - 0.16748867;
    else
        y = 3.0817580 * x3 - 5.87338670 * x2 + 3.75112997 * x - 0.37001483;

    // Convert xy to XYZ (Y=1)
    if (y < 1e-10) y = 1e-10;
    double X = x / y;
    double Y = 1.0;
    double Z = (1.0 - x - y) / y;

    // XYZ to linear sRGB (D65 illuminant)
    double R =  3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z;
    double G = -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z;
    double B =  0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z;

    // We want multipliers that, when applied to an image shot at this temperature,
    // will neutralize the color cast. So we compute what D65 (6500K) looks like
    // and divide by the target illuminant.
    // Instead, we compute the illuminant's RGB and take the inverse, normalized by green.
    if (R < 1e-6) R = 1e-6;
    if (G < 1e-6) G = 1e-6;
    if (B < 1e-6) B = 1e-6;

    // Multipliers are inverse of illuminant color, normalized so green=1
    r_mul = static_cast<float>(G / R);
    g_mul = 1.0f;
    b_mul = static_cast<float>(G / B);

    // Apply tint adjustment (green-magenta axis)
    // Tint > 0 pushes toward magenta (reduce green), tint < 0 pushes toward green
    // Scale: tint in roughly [-150, 150] range
    float tint_factor = 1.0f + tint * 0.005f;
    tint_factor = std::clamp(tint_factor, 0.2f, 5.0f);
    // Tint affects the green channel relative to R and B
    g_mul /= tint_factor;

    // Re-normalize so green = 1
    float inv_g = 1.0f / g_mul;
    r_mul *= inv_g;
    b_mul *= inv_g;
    g_mul = 1.0f;
}

void WhiteBalanceNode::process(Tile& tile, const EditRecipe& recipe)
{
    // The camera WB is already applied during demosaic. This node applies a
    // RELATIVE correction: how the user's chosen temperature differs from the
    // default (5500K). At default settings, multipliers should be [1,1,1].
    static constexpr float DEFAULT_TEMP = 5500.0f;
    static constexpr float DEFAULT_TINT = 0.0f;

    // Skip if at defaults
    if (std::abs(recipe.wb_temperature - DEFAULT_TEMP) < 1.0f &&
        std::abs(recipe.wb_tint - DEFAULT_TINT) < 0.1f)
    {
        VEGA_LOG_DEBUG("WhiteBalanceNode: at defaults ({}K, tint {}), skipping",
                       recipe.wb_temperature, recipe.wb_tint);
        return;
    }

    // Compute multipliers for default and for the user's chosen temperature
    float def_r, def_g, def_b;
    temperatureTintToRGB(DEFAULT_TEMP, DEFAULT_TINT, def_r, def_g, def_b);

    float usr_r, usr_g, usr_b;
    temperatureTintToRGB(recipe.wb_temperature, recipe.wb_tint, usr_r, usr_g, usr_b);

    // Relative correction = user / default
    float r_mul = usr_r / def_r;
    float g_mul = usr_g / def_g;
    float b_mul = usr_b / def_b;

    VEGA_LOG_DEBUG("WhiteBalanceNode: temp={}K tint={} -> relative muls=[{:.3f}, {:.3f}, {:.3f}]",
                   recipe.wb_temperature, recipe.wb_tint, r_mul, g_mul, b_mul);

    const uint32_t rows = tile.height;
    const uint32_t cols = tile.width;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    for (uint32_t row = 0; row < rows; ++row) {
        float* rowPtr = tile.data + row * stride;
        for (uint32_t col = 0; col < cols; ++col) {
            float* px = rowPtr + col * ch;
            px[0] *= r_mul;
            px[1] *= g_mul;
            px[2] *= b_mul;
        }
    }
}

} // namespace vega
