#pragma once
#include "pipeline/IProcessNode.h"

namespace vega {

class ExposureNode : public IProcessNode {
public:
    std::string_view name() const override { return "Exposure"; }
    PipelineStage stage() const override { return PipelineStage::Exposure; }
    void process(Tile& tile, const EditRecipe& recipe) override;

private:
    // Compute luminance from linear RGB
    static float luminance(float r, float g, float b);

    // S-curve contrast centered at mid, with strength [-100, 100]
    static float contrastCurve(float x, float strength);

    // Highlights recovery: attenuate bright pixels
    static float highlightsCurve(float lum, float amount);

    // Shadows lift: brighten dark pixels
    static float shadowsCurve(float lum, float amount);
};

} // namespace vega
