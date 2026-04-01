#pragma once
#include "pipeline/IProcessNode.h"
#include <array>

namespace vega {

class HSLNode : public IProcessNode {
public:
    std::string_view name() const override { return "HSL"; }
    PipelineStage stage() const override { return PipelineStage::HSL | PipelineStage::Vibrance; }
    void process(Tile& tile, const EditRecipe& recipe) override;

private:
    // RGB <-> HSL conversions (H in [0,360), S and L in [0,1])
    static void rgbToHSL(float r, float g, float b, float& h, float& s, float& l);
    static void hslToRGB(float h, float s, float l, float& r, float& g, float& b);

    // Compute blending weights for 8 HSL channels based on hue angle.
    // Channel centers: R=0, O=30, Y=60, G=120, Aqua=180, B=240, Purple=270, Mag=300
    static void computeChannelWeights(float hue_deg, std::array<float, 8>& weights);
};

} // namespace vega
