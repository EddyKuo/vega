#include "pipeline/cpu/ToneCurveNode.h"
#include "pipeline/EditRecipe.h"
#include "core/Logger.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace vega {

bool ToneCurveNode::isIdentityCurve(const std::vector<CurvePoint>& points)
{
    if (points.size() != 2)
        return false;
    return (std::abs(points[0].x) < 1e-5f && std::abs(points[0].y) < 1e-5f &&
            std::abs(points[1].x - 1.0f) < 1e-5f && std::abs(points[1].y - 1.0f) < 1e-5f);
}

void ToneCurveNode::buildLUT(const std::vector<CurvePoint>& points,
                             std::array<float, LUT_SIZE>& lut)
{
    // Handle degenerate cases
    if (points.empty()) {
        for (size_t i = 0; i < LUT_SIZE; ++i)
            lut[i] = static_cast<float>(i) / static_cast<float>(LUT_SIZE - 1);
        return;
    }

    if (points.size() == 1) {
        for (size_t i = 0; i < LUT_SIZE; ++i)
            lut[i] = std::clamp(points[0].y, 0.0f, 1.0f);
        return;
    }

    // Sort points by x coordinate
    std::vector<CurvePoint> sorted = points;
    std::sort(sorted.begin(), sorted.end(),
              [](const CurvePoint& a, const CurvePoint& b) { return a.x < b.x; });

    // Remove duplicates (keep last for each x)
    std::vector<float> xs, ys;
    xs.reserve(sorted.size());
    ys.reserve(sorted.size());
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (!xs.empty() && std::abs(sorted[i].x - xs.back()) < 1e-7f) {
            ys.back() = sorted[i].y; // overwrite duplicate
        } else {
            xs.push_back(sorted[i].x);
            ys.push_back(sorted[i].y);
        }
    }

    size_t n = xs.size();

    if (n == 1) {
        for (size_t i = 0; i < LUT_SIZE; ++i)
            lut[i] = std::clamp(ys[0], 0.0f, 1.0f);
        return;
    }

    // Monotone cubic Hermite interpolation (Fritsch-Carlson method)
    // Step 1: Compute slopes of secant lines
    std::vector<float> delta(n - 1);
    std::vector<float> h(n - 1);
    for (size_t i = 0; i < n - 1; ++i) {
        h[i] = xs[i + 1] - xs[i];
        if (h[i] < 1e-10f) h[i] = 1e-10f;
        delta[i] = (ys[i + 1] - ys[i]) / h[i];
    }

    // Step 2: Initialize tangents
    std::vector<float> m(n, 0.0f);
    m[0] = delta[0];
    m[n - 1] = delta[n - 2];
    for (size_t i = 1; i < n - 1; ++i) {
        if (delta[i - 1] * delta[i] <= 0.0f) {
            m[i] = 0.0f; // local extremum
        } else {
            m[i] = (delta[i - 1] + delta[i]) * 0.5f;
        }
    }

    // Step 3: Fritsch-Carlson monotonicity correction
    for (size_t i = 0; i < n - 1; ++i) {
        if (std::abs(delta[i]) < 1e-10f) {
            m[i] = 0.0f;
            m[i + 1] = 0.0f;
        } else {
            float alpha = m[i] / delta[i];
            float beta = m[i + 1] / delta[i];
            // Ensure we stay in the monotonicity region
            float sum_sq = alpha * alpha + beta * beta;
            if (sum_sq > 9.0f) {
                float tau = 3.0f / std::sqrt(sum_sq);
                m[i] = tau * alpha * delta[i];
                m[i + 1] = tau * beta * delta[i];
            }
        }
    }

    // Step 4: Evaluate the cubic Hermite spline at each LUT entry
    for (size_t i = 0; i < LUT_SIZE; ++i) {
        float x = static_cast<float>(i) / static_cast<float>(LUT_SIZE - 1);

        // Clamp/extrapolate outside the control point range
        if (x <= xs[0]) {
            lut[i] = std::clamp(ys[0], 0.0f, 1.0f);
            continue;
        }
        if (x >= xs[n - 1]) {
            lut[i] = std::clamp(ys[n - 1], 0.0f, 1.0f);
            continue;
        }

        // Binary search for the interval
        size_t lo = 0, hi = n - 1;
        while (hi - lo > 1) {
            size_t mid = (lo + hi) / 2;
            if (xs[mid] <= x)
                lo = mid;
            else
                hi = mid;
        }

        // Hermite basis on [xs[lo], xs[lo+1]]
        float dx = xs[lo + 1] - xs[lo];
        if (dx < 1e-10f) {
            lut[i] = std::clamp(ys[lo], 0.0f, 1.0f);
            continue;
        }

        float t = (x - xs[lo]) / dx;
        float t2 = t * t;
        float t3 = t2 * t;

        float h00 = 2.0f * t3 - 3.0f * t2 + 1.0f;
        float h10 = t3 - 2.0f * t2 + t;
        float h01 = -2.0f * t3 + 3.0f * t2;
        float h11 = t3 - t2;

        float val = h00 * ys[lo] + h10 * dx * m[lo] +
                    h01 * ys[lo + 1] + h11 * dx * m[lo + 1];

        lut[i] = std::clamp(val, 0.0f, 1.0f);
    }
}

float ToneCurveNode::evalLUT(const std::array<float, LUT_SIZE>& lut, float x)
{
    x = std::clamp(x, 0.0f, 1.0f);
    float idx = x * static_cast<float>(LUT_SIZE - 1);
    size_t i0 = static_cast<size_t>(idx);
    size_t i1 = (i0 < LUT_SIZE - 1) ? i0 + 1 : i0;
    float frac = idx - static_cast<float>(i0);
    return lut[i0] + (lut[i1] - lut[i0]) * frac;
}

void ToneCurveNode::process(Tile& tile, const EditRecipe& recipe)
{
    bool hasRGB = !isIdentityCurve(recipe.tone_curve_rgb);
    bool hasR   = !isIdentityCurve(recipe.tone_curve_r);
    bool hasG   = !isIdentityCurve(recipe.tone_curve_g);
    bool hasB   = !isIdentityCurve(recipe.tone_curve_b);

    if (!hasRGB && !hasR && !hasG && !hasB) {
        VEGA_LOG_DEBUG("ToneCurveNode: all curves are identity, skipping");
        return;
    }

    // Build LUTs
    std::array<float, LUT_SIZE> lutRGB, lutR, lutG, lutB;

    if (hasRGB) buildLUT(recipe.tone_curve_rgb, lutRGB);
    if (hasR)   buildLUT(recipe.tone_curve_r, lutR);
    if (hasG)   buildLUT(recipe.tone_curve_g, lutG);
    if (hasB)   buildLUT(recipe.tone_curve_b, lutB);

    VEGA_LOG_DEBUG("ToneCurveNode: applying curves (RGB={} R={} G={} B={})",
                   hasRGB, hasR, hasG, hasB);

    const uint32_t rows = tile.height;
    const uint32_t cols = tile.width;
    const uint32_t stride = tile.stride;
    const uint32_t ch = tile.channels;

    for (uint32_t row = 0; row < rows; ++row) {
        float* rowPtr = tile.data + row * stride;
        for (uint32_t col = 0; col < cols; ++col) {
            float* px = rowPtr + col * ch;

            // Apply master RGB curve first
            if (hasRGB) {
                px[0] = evalLUT(lutRGB, px[0]);
                px[1] = evalLUT(lutRGB, px[1]);
                px[2] = evalLUT(lutRGB, px[2]);
            }

            // Then per-channel curves
            if (hasR) px[0] = evalLUT(lutR, px[0]);
            if (hasG) px[1] = evalLUT(lutG, px[1]);
            if (hasB) px[2] = evalLUT(lutB, px[2]);
        }
    }
}

} // namespace vega
