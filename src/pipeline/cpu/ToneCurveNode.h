#pragma once
#include "pipeline/IProcessNode.h"
#include <array>
#include <vector>

namespace vega {

struct CurvePoint;

class ToneCurveNode : public IProcessNode {
public:
    std::string_view name() const override { return "ToneCurve"; }
    PipelineStage stage() const override { return PipelineStage::ToneCurve; }
    void process(Tile& tile, const EditRecipe& recipe) override;

private:
    static constexpr size_t LUT_SIZE = 4096;

    // Build a monotone cubic spline LUT from control points
    static void buildLUT(const std::vector<CurvePoint>& points,
                         std::array<float, LUT_SIZE>& lut);

    // Evaluate LUT with linear interpolation
    static float evalLUT(const std::array<float, LUT_SIZE>& lut, float x);

    // Check if the curve is an identity (only 2 points at {0,0} and {1,1})
    static bool isIdentityCurve(const std::vector<CurvePoint>& points);
};

} // namespace vega
