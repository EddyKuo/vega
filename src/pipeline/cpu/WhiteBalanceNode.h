#pragma once
#include "pipeline/IProcessNode.h"

namespace vega {

class WhiteBalanceNode : public IProcessNode {
public:
    std::string_view name() const override { return "WhiteBalance"; }
    PipelineStage stage() const override { return PipelineStage::WhiteBalance; }
    void process(Tile& tile, const EditRecipe& recipe) override;

private:
    // Convert color temperature (Kelvin) and tint to RGB multipliers
    // using Planckian locus approximation (Hernandez-Andres et al.)
    static void temperatureTintToRGB(float temperature_k, float tint,
                                     float& r_mul, float& g_mul, float& b_mul);
};

} // namespace vega
