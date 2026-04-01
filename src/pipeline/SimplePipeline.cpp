#include "pipeline/SimplePipeline.h"
#include <cmath>
#include <algorithm>
#include <cstring>

namespace vega {

// ── Bayer pattern channel indices ──
// Pattern encodes which color filter is at (row%2, col%2):
//   RGGB = 0: (0,0)=R  (0,1)=G  (1,0)=G  (1,1)=B
//   BGGR = 1: (0,0)=B  (0,1)=G  (1,0)=G  (1,1)=R
//   GRBG = 2: (0,0)=G  (0,1)=R  (1,0)=B  (1,1)=G
//   GBRG = 3: (0,0)=G  (0,1)=B  (1,0)=R  (1,1)=G

// Returns the color channel (0=R, 1=G, 2=B) at position (row, col) for a given pattern.
static int bayerChannel(uint32_t pattern, int row, int col)
{
    // Lookup table: pattern -> channel at each of the 4 CFA positions
    // Index: [pattern][row%2 * 2 + col%2]
    static constexpr int table[4][4] = {
        {0, 1, 1, 2},  // RGGB
        {2, 1, 1, 0},  // BGGR
        {1, 0, 2, 1},  // GRBG
        {1, 2, 0, 1},  // GBRG
    };
    uint32_t p = pattern < 4 ? pattern : 0;
    return table[p][(row & 1) * 2 + (col & 1)];
}

void SimplePipeline::demosaic(const float* bayer, float* rgb,
                              uint32_t width, uint32_t height, uint32_t bayer_pattern)
{
    auto clampCoord = [](int v, int maxVal) -> int {
        return v < 0 ? 0 : (v >= maxVal ? maxVal - 1 : v);
    };

    auto sample = [&](int r, int c) -> float {
        r = clampCoord(r, static_cast<int>(height));
        c = clampCoord(c, static_cast<int>(width));
        return bayer[r * width + c];
    };

    int w = static_cast<int>(width);
    int h = static_cast<int>(height);

    for (int r = 0; r < h; ++r)
    {
        for (int c = 0; c < w; ++c)
        {
            int ch = bayerChannel(bayer_pattern, r, c);
            float center = sample(r, c);

            float R, G, B;

            if (ch == 0)
            {
                // Red pixel
                R = center;
                // Green: average of 4 cardinal neighbors
                G = (sample(r - 1, c) + sample(r + 1, c) +
                     sample(r, c - 1) + sample(r, c + 1)) * 0.25f;
                // Blue: average of 4 diagonal neighbors
                B = (sample(r - 1, c - 1) + sample(r - 1, c + 1) +
                     sample(r + 1, c - 1) + sample(r + 1, c + 1)) * 0.25f;
            }
            else if (ch == 2)
            {
                // Blue pixel
                B = center;
                // Green: average of 4 cardinal neighbors
                G = (sample(r - 1, c) + sample(r + 1, c) +
                     sample(r, c - 1) + sample(r, c + 1)) * 0.25f;
                // Red: average of 4 diagonal neighbors
                R = (sample(r - 1, c - 1) + sample(r - 1, c + 1) +
                     sample(r + 1, c - 1) + sample(r + 1, c + 1)) * 0.25f;
            }
            else
            {
                // Green pixel — need to determine which neighbors are R and which are B.
                G = center;

                // For green pixels, the adjacent pixels in cardinal directions alternate
                // between R and B depending on the row/column parity and pattern.
                int chUp    = bayerChannel(bayer_pattern, r - 1, c);
                int chLeft  = bayerChannel(bayer_pattern, r, c - 1);

                // Vertical neighbors share one color, horizontal neighbors share the other
                if (chUp == 0)
                {
                    // Up/down are Red, left/right are Blue
                    R = (sample(r - 1, c) + sample(r + 1, c)) * 0.5f;
                    B = (sample(r, c - 1) + sample(r, c + 1)) * 0.5f;
                }
                else if (chUp == 2)
                {
                    // Up/down are Blue, left/right are Red
                    B = (sample(r - 1, c) + sample(r + 1, c)) * 0.5f;
                    R = (sample(r, c - 1) + sample(r, c + 1)) * 0.5f;
                }
                else if (chLeft == 0)
                {
                    // Left/right are Red, up/down are Blue
                    R = (sample(r, c - 1) + sample(r, c + 1)) * 0.5f;
                    B = (sample(r - 1, c) + sample(r + 1, c)) * 0.5f;
                }
                else
                {
                    // Left/right are Blue, up/down are Red
                    B = (sample(r, c - 1) + sample(r, c + 1)) * 0.5f;
                    R = (sample(r - 1, c) + sample(r + 1, c)) * 0.5f;
                }
            }

            size_t idx = (static_cast<size_t>(r) * width + c) * 3;
            rgb[idx + 0] = R;
            rgb[idx + 1] = G;
            rgb[idx + 2] = B;
        }
    }
}

void SimplePipeline::whiteBalance(float* rgb, uint32_t pixel_count,
                                  const float wb_mul[4])
{
    // Normalize multipliers by green channel (average of G1 and G2)
    float g_norm = (wb_mul[1] + wb_mul[3]) * 0.5f;
    if (g_norm <= 0.0f)
        g_norm = 1.0f;

    float r_scale = wb_mul[0] / g_norm;
    float g_scale = 1.0f;  // green is the reference
    float b_scale = wb_mul[2] / g_norm;

    for (uint32_t i = 0; i < pixel_count; ++i)
    {
        rgb[i * 3 + 0] *= r_scale;
        rgb[i * 3 + 1] *= g_scale;
        rgb[i * 3 + 2] *= b_scale;
    }
}

void SimplePipeline::colorTransform(float* rgb, uint32_t pixel_count,
                                    const float cam_to_xyz[9])
{
    // XYZ -> linear sRGB matrix (IEC 61966-2-1, D65)
    static constexpr float xyz_to_srgb[9] = {
         3.2404542f, -1.5371385f, -0.4985314f,
        -0.9692660f,  1.8760108f,  0.0415560f,
         0.0556434f, -0.2040259f,  1.0572252f
    };

    // Check if cam_to_xyz is all zeros (no matrix provided)
    bool has_matrix = false;
    for (int i = 0; i < 9; ++i)
    {
        if (cam_to_xyz[i] != 0.0f)
        {
            has_matrix = true;
            break;
        }
    }

    if (!has_matrix)
    {
        // No camera matrix available — skip color transform, assume sRGB already.
        return;
    }

    // Compose: combined = xyz_to_srgb * cam_to_xyz
    // This converts directly from camera RGB to linear sRGB.
    float combined[9] = {};
    for (int r = 0; r < 3; ++r)
    {
        for (int c = 0; c < 3; ++c)
        {
            combined[r * 3 + c] =
                xyz_to_srgb[r * 3 + 0] * cam_to_xyz[0 * 3 + c] +
                xyz_to_srgb[r * 3 + 1] * cam_to_xyz[1 * 3 + c] +
                xyz_to_srgb[r * 3 + 2] * cam_to_xyz[2 * 3 + c];
        }
    }

    for (uint32_t i = 0; i < pixel_count; ++i)
    {
        float* p = rgb + i * 3;
        float in_r = p[0], in_g = p[1], in_b = p[2];

        p[0] = combined[0] * in_r + combined[1] * in_g + combined[2] * in_b;
        p[1] = combined[3] * in_r + combined[4] * in_g + combined[5] * in_b;
        p[2] = combined[6] * in_r + combined[7] * in_g + combined[8] * in_b;
    }
}

float SimplePipeline::linearToSRGB(float x)
{
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;
    if (x < 0.0031308f)
        return 12.92f * x;
    return 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

void SimplePipeline::toRGBA8(const float* rgb, uint8_t* rgba, uint32_t pixel_count)
{
    for (uint32_t i = 0; i < pixel_count; ++i)
    {
        float r = std::clamp(rgb[i * 3 + 0], 0.0f, 1.0f);
        float g = std::clamp(rgb[i * 3 + 1], 0.0f, 1.0f);
        float b = std::clamp(rgb[i * 3 + 2], 0.0f, 1.0f);

        rgba[i * 4 + 0] = static_cast<uint8_t>(linearToSRGB(r) * 255.0f + 0.5f);
        rgba[i * 4 + 1] = static_cast<uint8_t>(linearToSRGB(g) * 255.0f + 0.5f);
        rgba[i * 4 + 2] = static_cast<uint8_t>(linearToSRGB(b) * 255.0f + 0.5f);
        rgba[i * 4 + 3] = 255;
    }
}

std::vector<uint8_t> SimplePipeline::process(const RawImage& raw)
{
    if (raw.bayer_data.empty() || raw.width == 0 || raw.height == 0)
        return {};

    uint32_t pixel_count = raw.width * raw.height;

    // Step 1: Bilinear demosaic — Bayer to RGB float
    std::vector<float> rgb(static_cast<size_t>(pixel_count) * 3);
    demosaic(raw.bayer_data.data(), rgb.data(),
             raw.width, raw.height, raw.metadata.bayer_pattern);

    // Step 2: White balance
    whiteBalance(rgb.data(), pixel_count, raw.wb_multipliers);

    // Step 3: Color transform (camera RGB -> XYZ -> linear sRGB)
    colorTransform(rgb.data(), pixel_count, raw.color_matrix);

    // Step 4: Clamp to [0,1] then apply sRGB gamma and convert to RGBA8
    std::vector<uint8_t> rgba(static_cast<size_t>(pixel_count) * 4);
    toRGBA8(rgb.data(), rgba.data(), pixel_count);

    return rgba;
}

} // namespace vega
